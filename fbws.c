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

#include <fb_draw.h>
#include <linux/uaccess.h>


#include "fbws.h"

static struct our_function our_cfg_script[] = {
	{ WS_START, WS_START},
	{ WS_CMD,   WS_SWRESET},
	{ WS_DELAY, 150},
	{ WS_CMD,  WS_SLPOUT},
	{ WS_DELAY, 500},
	{ WS_CMD, WS_FRMCTR1},
	{ WS_DATA, 0x01},
	{ WS_DATA, 0x2c},
	{ WS_DATA, 0x2d},
	{ WS_CMD,  WS_FRMCTR2},
	{ WS_DATA, 0x01},
	{ WS_DATA, 0x2c},
	{ WS_DATA, 0x2d},
	{ WS_CMD,  WS_FRMCTR3},
	{ WS_DATA, 0x01},
	{ WS_DATA, 0x2c},
	{ WS_DATA, 0x2d},
	{ WS_DATA, 0x01},
	{ WS_DATA, 0x2c},
	{ WS_DATA, 0x2d},
	{ WS_CMD,  WS_INVCTR},
	{ WS_DATA, 0x07},
	{ WS_CMD,  WS_PWCTR1},
	{ WS_DATA, 0xa2},
	{ WS_DATA, 0x02},
	{ WS_DATA, 0x84},
	{ WS_CMD,  WS_PWCTR2},
	{ WS_DATA, 0xc5},
	{ WS_CMD,  WS_PWCTR3},
	{ WS_DATA, 0x0a},
	{ WS_DATA, 0x00},
	{ WS_CMD,  WS_PWCTR4},
	{ WS_DATA, 0x8a},
	{ WS_DATA, 0x2a},
	{ WS_CMD,  WS_PWCTR5},
	{ WS_DATA, 0x8a},
	{ WS_DATA, 0xee},
	{ WS_CMD,  WS_VMCTR1},
	{ WS_DATA, 0x0e},
	{ WS_CMD,  WS_INVOFF},
	{ WS_CMD,  WS_MADCTL},
	{ WS_DATA, 0xc8},
	{ WS_CMD,  WS_COLMOD},
	{ WS_DATA, 0x05},
	{ WS_CMD,  WS_CASET},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x7f},
	{ WS_CMD,  WS_RASET},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x9f},
	{ WS_CMD,  WS_GMCTRP1},
	{ WS_DATA, 0x02},
	{ WS_DATA, 0x1c},
	{ WS_DATA, 0x07},
	{ WS_DATA, 0x12},
	{ WS_DATA, 0x37},
	{ WS_DATA, 0x32},
	{ WS_DATA, 0x29},
	{ WS_DATA, 0x2d},
	{ WS_DATA, 0x29},
	{ WS_DATA, 0x25},
	{ WS_DATA, 0x2b},
	{ WS_DATA, 0x39},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x01},
	{ WS_DATA, 0x03},
	{ WS_DATA, 0x10},
	{ WS_CMD,  WS_GMCTRN1},
	{ WS_DATA, 0x03},
	{ WS_DATA, 0x1d},
	{ WS_DATA, 0x07},
	{ WS_DATA, 0x06},
	{ WS_DATA, 0x2e},
	{ WS_DATA, 0x2c},
	{ WS_DATA, 0x29},
	{ WS_DATA, 0x2d},
	{ WS_DATA, 0x2e},
	{ WS_DATA, 0x2e},
	{ WS_DATA, 0x37},
	{ WS_DATA, 0x3f},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x00},
	{ WS_DATA, 0x02},
	{ WS_DATA, 0x10},
	{ WS_CMD,  WS_DISPON},
	{ WS_DELAY, 100},
	{ WS_CMD,  WS_NORON},
	{ WS_DELAY, 10},
	{ WS_END, WS_END},
};


static struct fb_fix_screeninfo ourfb_fix ={
	.id =		"waveshare213", 
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_PSEUDOCOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0, 
	.line_length =	WIDTH*BPP/8,
	.accel =	FB_ACCEL_NONE,
};



static struct fb_var_screeninfo ourfb_var = {
	.xres =			WIDTH,
	.yres =			HEIGHT,
	.xres_virtual =		WIDTH,
	.yres_virtual =		HEIGHT,
	.bits_per_pixel =	BPP,
	.nonstd	=		1,
};

static int our_write(struct ourfb_par *par, u8 data)
{
	u8 txbuf[2]; /* allocation from stack must go */

	txbuf[0] = data;

	return spi_write(par->spi, &txbuf[0], 1);
}

static void our_write_data(struct ourfb_par *par, u8 data)
{
	int ret = 0;

	/* Set data mode */
	gpio_set_value(par->dc, 1);

	ret = our_write(par, data);
	if (ret < 0)
		pr_err("%s: write data %02x failed with status %d\n",
			par->info->fix.id, data, ret);
}

static int our_write_data_buf(struct ourfb_par *par,
					u8 *txbuf, int size)
{
	/* Set data mode */
	gpio_set_value(par->dc, 1);

	/* Write entire buffer */
	return spi_write(par->spi, txbuf, size);
}


static void our_write_cmd(struct ourfb_par *par, u8 data)
{
	int ret = 0;

	/* Set command mode */
	gpio_set_value(par->dc, 0);

	ret = our_write(par, data);
	if (ret < 0)
		pr_err("%s: write command %02x failed with status %d\n",
			par->info->fix.id, data, ret);
}

static void our_run_cfg_script(struct ourfb_par *par)
{
	int i = 0;
	int end_script = 0;

	do {
		switch (our_cfg_script[i].cmd)
		{
		case WS_START:
			break;
		case WS_CMD:
			our_write_cmd(par,
				our_cfg_script[i].data & 0xff);
			break;
		case WS_DATA:
			our_write_data(par,
				our_cfg_script[i].data & 0xff);
			break;
		case WS_DELAY:
			mdelay(our_cfg_script[i].data);
			break;
		case WS_END:
			end_script = 1;
		}
		i++;
	} while (!end_script);
}

static void our_set_addr_win(struct ourfb_par *par,
				int xs, int ys, int xe, int ye)
{
	our_write_cmd(par, WS_CASET);
	our_write_data(par, 0x00);
	our_write_data(par, xs+2);
	our_write_data(par, 0x00);
	our_write_data(par, xe+2);
	our_write_cmd(par, WS_RASET);
	our_write_data(par, 0x00);
	our_write_data(par, ys+1);
	our_write_data(par, 0x00);
	our_write_data(par, ye+1);
}

static void our_reset(struct ourfb_par *par)
{
	/* Reset controller */
	gpio_set_value(par->rst, 0);
	udelay(10);
	gpio_set_value(par->rst, 1);
	mdelay(120);
}

static void ourfb_update_display(struct ourfb_par *par)
{
	int ret = 0;
	u8 *vmem = par->info->screen_base;
#ifdef __LITTLE_ENDIAN
	int i;
	u16 *vmem16 = (u16 *)vmem;
	u16 *ssbuf = par->ssbuf;

	for (i=0; i<WIDTH*HEIGHT*BPP/8/2; i++)
		ssbuf[i] = swab16(vmem16[i]);
#endif
	/*
		TODO:
		Allow a subset of pages to be passed in
		(for deferred I/O).  Check pages against
		pan display settings to see if they
		should be updated.
	*/
	/* For now, just write the full 40KiB on each update */

	/* Set row/column data window */
	our_set_addr_win(par, 0, 0, WIDTH-1, HEIGHT-1);

	/* Internal RAM write command */
	our_write_cmd(par, WS_RAMWR);

	/* Blast framebuffer to ST7735 internal display RAM */
#ifdef __LITTLE_ENDIAN
	ret = our_write_data_buf(par, (u8 *)ssbuf, WIDTH*HEIGHT*BPP/8);
#else
	ret = our_write_data_buf(par, vmem, WIDTH*HEIGHT*BPP/8);
#endif
	if (ret < 0)
		pr_err("%s: spi_write failed to update display buffer\n",
			par->info->fix.id);
}





static int ourfb_init_display(struct ourfb_par *par)
{
	/* TODO: Need some error checking on gpios */

        /* Request GPIOs and initialize to default values */
        gpio_request_one(par->rst, GPIOF_OUT_INIT_HIGH,
			"WaveShare Reset Pin");
        gpio_request_one(par->dc, GPIOF_OUT_INIT_LOW,
			"WaveShare Data/Command Pin");

	our_reset(par);

	our_run_cfg_script(par);

	return 0;
}

void ourfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct ourfb_par *par = info->par;

	sys_fillrect(info, rect);

	ourfb_update_display(par);
}

void ourfb_copyarea(struct fb_info *info, const struct fb_copyarea *area) 
{
	struct ourfb_par *par = info->par;

	sys_copyarea(info, area);

	ourfb_update_display(par);
}

void ourfb_imageblit(struct fb_info *info, const struct fb_image *image) 
{
	struct ourfb_par *par = info->par;

	sys_imageblit(info, image);

	ourfb_update_display(par);
}



static ssize_t ourfb_read(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct outfb_par *par = info->par;
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

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	ourfb_update_display(par);

	return (err) ? err : count;
}


static ssize_t ourfb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct outfb_par *par = info->par;
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

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	ourfb_update_display(par);

	return (err) ? err : count;
}


static struct fb_ops ourfb_ops = {
	.owner		= THIS_MODULE,
	//.fb_read	= fb_sys_read,
	.fb_read	= ourfb_read,
	.fb_write	= ourfb_write,
	.fb_fillrect	= ourfb_fillrect,
	.fb_copyarea	= ourfb_copyarea,
	.fb_imageblit	= ourfb_imageblit,
};


static void ourfb_deferred_io(struct fb_info *info,	struct list_head *pagelist)
{
	ourfb_update_display(info->par);
}

// static struct fb_deferred_io ourfb_defio = {
// 	.delay		= HZ,
// 	.deferred_io	= ourfb_deferred_io,
// };

//Old source from pci
// static struct spi_device_id ourfb_spi_tbl[]={
// 	{SPI_VENDOR_ID_123,SPI_DEVICE_ID_123,SPI_ANY_ID,SPI_ANY_ID},
// 	{0}
// };

static int ourfb_spi_init(struct spi_device *spi)
{
	printk("start ourfb_spi_init\n");
	struct fb_info *info;
	int retval = -ENOMEM;

	struct ourfb_platform_data *pdata = spi->dev.platform_data;

	int vmem_size = WIDTH*HEIGHT*BPP/8;
	u8 *vmem;
	struct ourfb_par *par;

	printk("start vmem:\n");
	vmem = vzalloc(vmem_size);
	if (!vmem)
		return retval;

	info =framebuffer_alloc(sizeof(struct ourfb_par),&spi ->dev);
	if (!info)
	{
		printk("Error in alloc:\n");
		goto fballoc_fail;
	}


	info->screen_base = (u8 __force __iomem *)vmem;
	info->fbops = &ourfb_ops;
	info->fix = ourfb_fix;
	info->fix.smem_len = vmem_size;
	info->var = ourfb_var;
	/* Choose any packed pixel format as long as it's RGB565 */
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->var.transp.offset = 0;
	info->var.transp.length = 0;
	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;
	//info->fbdefio = &ourfb_defio;

	//fb_deferred_io_init(info);

	par = info->par;
	par->info = info;
	par->spi = spi;
	par->rst = pdata->rst_gpio;
	par->dc = pdata->dc_gpio;

#ifdef __LITTLE_ENDIAN
	/* Allocate swapped shadow buffer */
	vmem = vzalloc(vmem_size);
	if (!vmem)
		return retval;
	par->ssbuf = vmem;
#endif



	retval = register_framebuffer(info);
	if (retval < 0)
		goto fbreg_fail;


	spi_set_drvdata(spi, info);

	retval = ourfb_init_display(par);
	if (retval < 0)
		goto init_fail;

	printk(KERN_INFO
		"fb%d: %s frame buffer device,\n\tusing %d KiB of video memory\n",
		info->node, info->fix.id, vmem_size);

	return 0;

/* TODO: release gpios on fail */
init_fail:
	spi_set_drvdata(spi, NULL);

fbreg_fail:
	framebuffer_release(info);

fballoc_fail:
	vfree(vmem);

	return retval;
}

static void ourfb_spi_remove(struct spi_device *spi)
{
	struct fb_info *p =spi_get_drvdata(spi);
	unregister_framebuffer(p);
	fb_dealloc_cmap(&p->cmap);
	iounmap(p->screen_base);
	framebuffer_release(p);

}


static struct spi_device_id ourfb_spi_tbl[]={
	{"waveshare213",0},
	{ },
};


 MODULE_DEVICE_TABLE(spi,ourfb_spi_tbl);


static struct spi_driver ourfb_driver={
	.driver={
		.name 		=	"waveshare213",
		.owner 		=THIS_MODULE,

	},
	
	.id_table	=	ourfb_spi_tbl,
	.probe		=	ourfb_spi_init,
	.remove 	=	ourfb_spi_remove, 
};


static int __init ourfb_init(void)
{
	printk("new init fb driver\n");
	
	return spi_register_driver(&ourfb_driver);

}

static void __exit ourfb_exit(void)
{
	printk("remove fb driver\n");
	spi_unregister_driver(&ourfb_driver);
}


// tatic int __init fbws_init(struct fb_info *fb_info)
// {
// 	struct fb_info *info;
// 	register_framebuffer(info);
// 	return 0;
// }

// static void __exit fbws_exit(void)
// {
// 	struct fb_info *p;
// 	unregister_framebuffer(p);
// 	framebuffer_release(p)

// }





module_init(ourfb_init);
module_exit(ourfb_exit);

MODULE_ALIAS("platform:eink");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thong Nguyen");
MODULE_DESCRIPTION("E-Ink Display driver");
MODULE_VERSION("0.1");
