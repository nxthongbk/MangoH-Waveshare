#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/fb.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/device.h>

#include "fb_waveshare_eink.h"

#define WS_SW_RESET								0x12
#define WS_DISPLAY_UPDATE_CONTROL_1				0x21
#define WS_DISPLAY_UPDATE_CONTROL_2				0x22
#define WS_WRITE_RAM							0x24
#define WS_WRITE_VCOM_REGISTER					0x2C
#define WS_WRITE_LUT_REGISTER					0x32
#define WS_SET_RAM_X_ADDRESS_START_END_POSITION	0x44
#define WS_SET_RAM_Y_ADDRESS_START_END_POSITION	0x45
#define WS_SET_RAM_X_ADDRESS_COUNTER			0x4E
#define WS_SET_RAM_Y_ADDRESS_COUNTER			0x4F
#define WS_MASTER_ACTIVATION					0x20
#define WS_TERMINATE_FRAME_READ_WRITE			0xFF
#define WS_DRIVER_OUTPUT_CONTROL				0x01
#define WS_BOOSTER_SOFT_START_CONTROL			0x0C
#define WS_DATA_ENTRY_MODE_SETTING				0x11
#define WS_SET_DUMMY_LINE_PERIOD				0x3A
#define WS_SET_GATE_TIME						0x3B

static unsigned width = 0;
static unsigned height = 0;
static unsigned bpp = 0;

struct ws_eink_fb_par {
	struct spi_device *spi;
	struct fb_info *info;
	u8 *ssbuf;
	int rst;
	int dc;
	int busy;
};

const u8 lut_full_update[] = {
	0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

const u8 lut_partial_update[] = {
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int ws_eink_write(struct ws_eink_fb_par *par, u8 data)
{
	u8 txbuf[1];
	txbuf[0] = data;
	return spi_write(par->spi, &txbuf[0], 1);
}

static void ws_eink_write_data(struct ws_eink_fb_par *par, u8 data)
{
	int ret = 0;
	gpio_set_value(par->dc, 1);

	ret = ws_eink_write(par, data);
	if (ret < 0) {
		printk("spi write data error\n");
		pr_err("%s: write data %02x failed with status %d\n",
		       par->info->fix.id, data, ret);
	}
}

static int ws_eink_write_data_buf(struct ws_eink_fb_par *par, const u8 *txbuf,
				size_t size)
{
	gpio_set_value(par->dc, 1);

	return spi_write(par->spi, txbuf, size);
}

static void ws_eink_write_cmd(struct ws_eink_fb_par *par, u8 cmd)
{
	int ret = 0;

	gpio_set_value(par->dc, 0);

	ret = ws_eink_write(par, cmd);
	if (ret < 0)
		pr_err("%s: write command %02x failed with status %d\n",
		       par->info->fix.id, cmd, ret);
}

static void wait_until_idle(struct ws_eink_fb_par *par)
{
	while (gpio_get_value(par->busy) != 0)
		mdelay(100);
}

static void ws_eink_reset(struct ws_eink_fb_par *par)
{
	gpio_set_value(par->rst, 0);
	mdelay(200);
	gpio_set_value(par->rst, 1);
	mdelay(200);
}

static void set_lut(struct ws_eink_fb_par *par, const u8 *lut,
					 size_t lut_size)
{
	int ret;
	ws_eink_write_cmd(par, WS_WRITE_LUT_REGISTER);
	ret = ws_eink_write_data_buf(par, lut, lut_size);
	if (ret != 0)
		dev_err(&par->spi->dev, "ws_eink_write_data_buf failed (%d)",
			ret);
}

static void int_lut(struct ws_eink_fb_par *par, const u8 *lut,
					 size_t lut_size)
{
	ws_eink_reset(par);
	ws_eink_write_cmd(par, WS_DRIVER_OUTPUT_CONTROL);
	ws_eink_write_data(par, (height - 1) & 0xFF);
	ws_eink_write_data(par, ((height - 1) >> 8) & 0xFF);
	ws_eink_write_data(par, 0x00);
	ws_eink_write_cmd(par, WS_BOOSTER_SOFT_START_CONTROL);
	ws_eink_write_data(par, 0xD7);
	ws_eink_write_data(par, 0xD6);
	ws_eink_write_data(par, 0x9D);
	ws_eink_write_cmd(par, WS_WRITE_VCOM_REGISTER);
	ws_eink_write_data(par, 0xA8);
	ws_eink_write_cmd(par, WS_SET_DUMMY_LINE_PERIOD);
	ws_eink_write_data(par, 0x1A);
	ws_eink_write_cmd(par, WS_SET_GATE_TIME);
	ws_eink_write_data(par, 0x08);
	ws_eink_write_cmd(par, WS_DATA_ENTRY_MODE_SETTING);
	ws_eink_write_data(par, 0x03);
	set_lut(par, lut, lut_size);
}

static void set_memory_area(struct ws_eink_fb_par *par, int x_start,
			    int y_start, int x_end, int y_end)
{
	ws_eink_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	ws_eink_write_data(par, (x_start >> 3) & 0xFF);
	ws_eink_write_data(par, (x_end >> 3) & 0xFF);

	ws_eink_write_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION);
	ws_eink_write_data(par, y_start & 0xFF);
	ws_eink_write_data(par, (y_start >> 8) & 0xFF);
	ws_eink_write_data(par, y_end & 0xFF);
	ws_eink_write_data(par, (y_end >> 8) & 0xFF);
}

static void set_memory_pointer(struct ws_eink_fb_par *par, int x, int y)
{
	ws_eink_write_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER);
	ws_eink_write_data(par, (x >> 3) & 0xFF);

	ws_eink_write_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER);
	ws_eink_write_data(par, y & 0xFF);
	ws_eink_write_data(par, (y >> 8) & 0xFF);
	wait_until_idle(par);
}

static void clear_frame_memory(struct ws_eink_fb_par *par, u8 color)
{
	int j;
	u8 solid_line[width / 8];
	memset(solid_line, color, ARRAY_SIZE(solid_line));
	set_memory_area(par, 0, 0, width - 1, height - 1);
	for (j = 0; j < height; j++) {
		int ret;
		set_memory_pointer(par, 0, j);
		ws_eink_write_cmd(par, WS_WRITE_RAM);
		ret = ws_eink_write_data_buf(par, solid_line,
					   ARRAY_SIZE(solid_line));
		if (ret != 0)
			dev_err(&par->spi->dev,
				"Failure while writing display memory (%d)",
				ret);
	}
}

static void set_frame_memory(struct ws_eink_fb_par *par, u8 *image_buffer)
{
	int row;
	set_memory_area(par, 0, 0, width - 1, height - 1);

	for (row = 0; row < height; row++) {
		int ret;
		const size_t row_bytes = width / 8;
		set_memory_pointer(par, 0, row);
		ws_eink_write_cmd(par, WS_WRITE_RAM);

		ret = ws_eink_write_data_buf(par,
					     &image_buffer[row * row_bytes],
					     row_bytes);
		if (ret != 0)
			dev_err(&par->spi->dev,
				"Failure while writing display memory (%d)",
				ret);
	}
}

static void display_frame(struct ws_eink_fb_par *par)
{
	ws_eink_write_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2);
	ws_eink_write_data(par, 0xC4);
	ws_eink_write_cmd(par, WS_MASTER_ACTIVATION);
	ws_eink_write_cmd(par, WS_TERMINATE_FRAME_READ_WRITE);

	wait_until_idle(par);
}

static void ws_eink_init_display(struct ws_eink_fb_par *par)
{
	gpio_request(par->rst, "sysfs");
	gpio_request(par->dc, "sysfs");
	gpio_request(par->busy, "sysfs");

	gpio_direction_output(par->dc, true);
	gpio_set_value(par->dc, 0);
	gpio_export(par->dc, true);

	gpio_direction_output(par->rst, true);
	gpio_set_value(par->rst, 0);
	gpio_export(par->rst, true);

	gpio_direction_input(par->busy);
	gpio_set_value(par->busy, 0);
	gpio_export(par->busy, true);

	int_lut(par, lut_full_update, ARRAY_SIZE(lut_full_update));

	clear_frame_memory(par, 0xFF);
	display_frame(par);

	int_lut(par, lut_partial_update, ARRAY_SIZE(lut_partial_update));
}

static void ws_eink_update_display(struct ws_eink_fb_par *par)
{
	u8 *vmem = par->info->screen_base;
	u8 *ssbuf = par->ssbuf;
	memcpy(&ssbuf, &vmem, sizeof(vmem));
	set_frame_memory(par, ssbuf);
	display_frame(par);
}

void ws_eink_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct ws_eink_fb_par *par = info->par;
	sys_fillrect(info, rect);
	ws_eink_update_display(par);
}

void ws_eink_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct ws_eink_fb_par *par = info->par;
	sys_copyarea(info, area);
	ws_eink_update_display(par);
}

void ws_eink_fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct ws_eink_fb_par *par = info->par;
	sys_imageblit(info, image);
	ws_eink_update_display(par);
}

static ssize_t ws_eink_fb_write(struct fb_info *info, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *)(info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	return (err) ? err : count;
}

static struct fb_ops ws_eink_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fb_sys_read,
	.fb_write	= ws_eink_fb_write,
	.fb_fillrect	= ws_eink_fb_fillrect,
	.fb_copyarea	= ws_eink_fb_copyarea,
	.fb_imageblit	= ws_eink_fb_imageblit,
};

enum waveshare_devices {
	DEV_WS_213,
	DEV_WS_27,
	DEV_WS_29,
	DEV_WS_42,
	DEV_WS_75,
};

struct waveshare_eink_device_properties {
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
};

static struct waveshare_eink_device_properties devices[] =
{
	[DEV_WS_213] = {.width = 128, .height = 250, .bpp = 1},
	[DEV_WS_27]  = {.width = 176, .height = 264, .bpp = 1},
	[DEV_WS_29]  = {.width = 128, .height = 296, .bpp = 1},
	[DEV_WS_42]  = {.width = 300, .height = 400, .bpp = 1},
	[DEV_WS_75]  = {.width = 384, .height = 640, .bpp = 1},
};

static struct spi_device_id waveshare_eink_tbl[] = {
	{ "waveshare_213", (kernel_ulong_t)&devices[DEV_WS_213] },
	{ "waveshare_27",  (kernel_ulong_t)&devices[DEV_WS_27] },
	{ "waveshare_29",  (kernel_ulong_t)&devices[DEV_WS_29] },
	{ "waveshare_42",  (kernel_ulong_t)&devices[DEV_WS_42] },
	{ "waveshare_75",  (kernel_ulong_t)&devices[DEV_WS_75] },
	{ },
};
MODULE_DEVICE_TABLE(spi, waveshare_eink_tbl);

static void ws_eink_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	ws_eink_update_display(info->par);
}

static struct fb_deferred_io ws_eink_defio = {
	.delay		= HZ / 30,
	.deferred_io	= ws_eink_deferred_io,
};

static int ws_eink_spi_probe(struct spi_device *spi)
{
	struct fb_info *info;
	int retval = 0;
	struct waveshare_eink_platform_data *pdata;
	const struct spi_device_id *spi_id;
	const struct waveshare_eink_device_properties *dev_props;
	struct ws_eink_fb_par *par;
	u8 *vmem;
	int vmem_size;

	pdata = spi->dev.platform_data;
	if (!pdata) {
		dev_err(&spi->dev, "Required platform data was not provided");
		return -EINVAL;
	}

	spi_id = spi_get_device_id(spi);
	if (!spi_id) {
		dev_err(&spi->dev, "device id not supported!\n");
		return -EINVAL;
	}

	dev_props = (const struct waveshare_eink_device_properties *)
		spi_id->driver_data;
	if (!dev_props) {
		dev_err(&spi->dev, "device definition lacks driver_data\n");
		return -EINVAL;
	}
	width = dev_props->width;
	height = dev_props->height;
	bpp = dev_props->bpp;

	vmem_size = width * height * bpp / 8;
	vmem = vzalloc(vmem_size);
	if (!vmem)
		return -ENOMEM;

	info = framebuffer_alloc(sizeof(struct ws_eink_fb_par), &spi->dev);
	if (!info) {
		retval = -ENOMEM;
		goto fballoc_fail;
	}

	info->screen_base = (u8 __force __iomem *)vmem;
	info->fbops = &ws_eink_ops;

	WARN_ON(strlcpy(info->fix.id, "waveshare_eink", sizeof(info->fix.id)) >=
		sizeof(info->fix.id));
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_PSEUDOCOLOR;
	info->fix.smem_len	= vmem_size;
	info->fix.xpanstep	= 0;
	info->fix.ypanstep	= 0;
	info->fix.ywrapstep	= 0;
	info->fix.line_length	= width * bpp / 8;

	info->var.xres			= width;
	info->var.yres			= height;
	info->var.xres_virtual		= width;
	info->var.yres_virtual		= height;
	info->var.bits_per_pixel	= bpp;

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	info->fbdefio = &ws_eink_defio;
	fb_deferred_io_init(info);

	par = info->par;
	par->info	= info;
	par->spi	= spi;
	par->rst	= pdata->rst_gpio;
	par->dc		= pdata->dc_gpio;
	par->busy	= pdata->busy_gpio;

	vmem = vzalloc(vmem_size);
	if (!vmem)
		return retval;
	par->ssbuf = vmem;

	retval = register_framebuffer(info);
	if (retval < 0) {
		dev_err(&spi->dev, "framebuffer registration failed");
		goto fbreg_fail;
	}

	spi_set_drvdata(spi, info);

	ws_eink_init_display(par);

	dev_dbg(&spi->dev,
		"fb%d: %s frame buffer device,\n\tusing %d KiB of video memory\n",
		info->node, info->fix.id, vmem_size);

	return 0;

fbreg_fail:
	framebuffer_release(info);

fballoc_fail:
	vfree(vmem);

	return retval;
}

static int ws_eink_spi_remove(struct spi_device *spi)
{
	struct fb_info *p = spi_get_drvdata(spi);
	unregister_framebuffer(p);
	fb_dealloc_cmap(&p->cmap);
	iounmap(p->screen_base);
	framebuffer_release(p);

	return 0;
}

static struct spi_driver ws_eink_driver = {
	.driver = {
		.name	= "waveshare_eink",
		.owner	= THIS_MODULE,
	},

	.id_table	= waveshare_eink_tbl,
	.probe		= ws_eink_spi_probe,
	.remove 	= ws_eink_spi_remove,
};
module_spi_driver(ws_eink_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thong Nguyen");
MODULE_DESCRIPTION("FB driver for Waveshare eink displays");
MODULE_VERSION("0.1");
