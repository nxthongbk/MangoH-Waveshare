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

#include "fb_waveshare_213.h"

#define DRVNAME		"waveshare_213"
#define WIDTH		0
#define HEIGHT		0
#define BPP			1

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
#define WS_DRIVER_OUTPUT_CONTROL                0x01
#define WS_BOOSTER_SOFT_START_CONTROL           0x0C
#define WS_DATA_ENTRY_MODE_SETTING              0x11                         
#define WS_SET_DUMMY_LINE_PERIOD                0x3A
#define WS_SET_GATE_TIME                        0x3B


static char *name;
module_param(name, charp, 0);
MODULE_PARM_DESC(name, "Device name.");

static unsigned width = 0;
module_param(width, uint, 0);
MODULE_PARM_DESC(width, "Display width, used with the custom argument");

static unsigned height = 0;
module_param(height, uint, 0);
MODULE_PARM_DESC(height, "Display height, used with the custom argument");

static unsigned bpp = 0;
module_param(bpp, uint, 0);
MODULE_PARM_DESC(buswidth, "Display bit per pixel, used with the custom argument");


const unsigned char lut_full_update[] =
{
    0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char lut_partial_update[] =
{
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



static struct fb_fix_screeninfo ws213fb_fix ={
	.id =		"waveshare213", 
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_PSEUDOCOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0, 
	.line_length =	WIDTH*BPP/8,
	.accel =	FB_ACCEL_NONE,
};



static struct fb_var_screeninfo ws213fb_var = {
	.xres =			WIDTH,
	.yres =			HEIGHT,
	.xres_virtual =		WIDTH,
	.yres_virtual =		HEIGHT,
	.bits_per_pixel =	BPP,
	.nonstd	=		1,
};

static int ws213_write(struct ws213fb_par *par, u8 data)
{
	u8 txbuf[2];
	txbuf[0] = data;
	return spi_write(par->spi,&txbuf[0],1);
}

static void ws213_write_data(struct ws213fb_par *par, u8 data)
{
	int ret = 0;
	gpio_set_value(par->dc, 1);
	
	ret = ws213_write(par, data);
	if (ret < 0)
	{
		printk("spi write data error\n");
		pr_err("%s: write data %02x failed with status %d\n",
			par->info->fix.id, data, ret);
	}
}

static int ws213_write_data_buf(struct ws213fb_par *par,
					u8 *txbuf, int size)
{
	gpio_set_value(par->dc, 1);
	
	return spi_write(par->spi, txbuf, size);
}


static void ws213_write_cmd(struct ws213fb_par *par, u8 data)
{
	int ret = 0;

	gpio_set_value(par->dc, 0);
	
	ret = ws213_write(par, data);
	if (ret < 0)
		pr_err("%s: write command %02x failed with status %d\n",
			par->info->fix.id, data, ret);
}


static void wait_until_idle(struct ws213fb_par *par)
{
	while(gpio_get_value(par->busy) != 0)
	{
		mdelay(100);
	}
}


static void ws213_reset(struct ws213fb_par *par)
{
	gpio_set_value(par->rst, 0);
	mdelay(200);
	gpio_set_value(par->rst, 1);
	mdelay(200);
}


static void set_lut(struct ws213fb_par *par,unsigned char* lut)
{
	 ws213_write_cmd(par, WS_WRITE_LUT_REGISTER);
	 int i;
	 for(i = 0;i < 30; i++)
	 {
 		ws213_write_data(par, lut[i]);
	 }
}


static int int_lut(struct ws213fb_par *par,unsigned char* lut)
{
	
	ws213_reset(par);
    ws213_write_cmd(par, WS_DRIVER_OUTPUT_CONTROL);
    ws213_write_data(par, (height - 1) & 0xFF);
    ws213_write_data(par, ((height - 1) >> 8) & 0xFF);
    ws213_write_data(par, 0x00);
    ws213_write_cmd(par, WS_BOOSTER_SOFT_START_CONTROL);
    ws213_write_data(par, 0xD7);
    ws213_write_data(par, 0xD6);
    ws213_write_data(par, 0x9D);
    ws213_write_cmd(par, WS_WRITE_VCOM_REGISTER);
    ws213_write_data(par, 0xA8);
    ws213_write_cmd(par, WS_SET_DUMMY_LINE_PERIOD);
    ws213_write_data(par, 0x1A);
    ws213_write_cmd(par, WS_SET_GATE_TIME);
    ws213_write_data(par, 0x08);
    ws213_write_cmd(par, WS_DATA_ENTRY_MODE_SETTING);
    ws213_write_data(par, 0x03);
    set_lut(par, lut);
    return 0;
}


static void set_memory_area(struct ws213fb_par *par,int x_start,int y_start,int x_end,int y_end)
{
	ws213_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	ws213_write_data(par, (x_start >> 3) & 0xFF);
	ws213_write_data(par, (x_end >> 3) & 0xFF);

	ws213_write_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION);
	ws213_write_data(par, y_start & 0xFF);
	ws213_write_data(par, (y_start >> 8) & 0xFF);
	ws213_write_data(par, y_end & 0xFF);
	ws213_write_data(par, (y_end >> 8) & 0xFF);
}


static void set_memory_pointer(struct ws213fb_par *par,int x, int y)
{
	ws213_write_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER);
	ws213_write_data(par, (x >> 3) & 0xFF);

	ws213_write_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER);
	ws213_write_data(par, y & 0xFF);
	ws213_write_data(par, (y >> 8) & 0xFF);
	wait_until_idle(par);
}


static void clear_frame_memory(struct ws213fb_par *par,unsigned char color)
{
	printk("call clear_frame_memory\n");
	set_memory_area(par, 0, 0, width-1, height-1);
	int j;
	for(j = 0; j < height ;j++)
	{
		set_memory_pointer(par,0 , j);
		ws213_write_cmd(par, WS_WRITE_RAM);
		int i;
		for(i = 0; i < width/8 ;i++)
		{
			ws213_write_data(par,color);			
		}
	}
}


static void set_frame_memory(struct ws213fb_par *par,unsigned char* image_buffer)
{
	set_memory_area(par, 0, 0, width-1, height-1);
	

	int j;
	for(j = 0; j < height ; j++)
	{
		set_memory_pointer(par,0 , j);
		ws213_write_cmd(par, WS_WRITE_RAM);

		int i;
		for(i = 0; i < (width-1)/8 ; i++)
		{
			ws213_write_data(par, image_buffer[i + j * (width/8)]);
		}
	}
}


static void display_frame(struct ws213fb_par *par)
{
	ws213_write_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2);
	ws213_write_data(par, 0xC4);
	ws213_write_cmd(par, WS_MASTER_ACTIVATION);
	ws213_write_cmd(par, WS_TERMINATE_FRAME_READ_WRITE);
	
	wait_until_idle(par);
}


static int ws213fb_init_display(struct ws213fb_par *par)
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


    if(int_lut(par,lut_full_update)!=0)
	 {
	 	printk("Init lut error");
	 } 
		
	clear_frame_memory(par,0xFF);
	display_frame(par);


    if(int_lut(par,lut_partial_update)!=0)
	 {
	 	printk("Init lut error");
	 } 
	
	return 0;
}


static void ws213fb_update_display(struct ws213fb_par *par)
{
	int ret = 0;
	u8 *vmem = par->info->screen_base;
#ifdef __LITTLE_ENDIAN
	printk("goto __LITTLE_ENDIAN\n");
 	int i;
	u8 *vmem8 = (u8 *)vmem;
 	u8 *ssbuf = par->ssbuf;

 	for (i=0; i<width*height*bpp/8; i++)
 	{
 		ssbuf[i] = vmem8[i];
 	}

 	set_frame_memory(par,ssbuf);
 	display_frame(par);

 #endif
	
}


void ws213fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct ws213fb_par *par = info->par;
	sys_fillrect(info, rect);
	ws213fb_update_display(par);
}

void ws213fb_copyarea(struct fb_info *info, const struct fb_copyarea *area) 
{
	struct ws213fb_par *par = info->par;
	sys_copyarea(info, area);
	ws213fb_update_display(par);
}

void ws213fb_imageblit(struct fb_info *info, const struct fb_image *image) 
{
	struct ws213fb_par *par = info->par;

	sys_imageblit(info, image);

	ws213fb_update_display(par);
}


static ssize_t ws213fb_write(struct fb_info *info, const char __user *buf,
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

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	
	return (err) ? err : count;
}


static struct fb_ops ws213fb_ops = {
	.owner			= THIS_MODULE,
	.fb_read		= fb_sys_read,
	.fb_write		= ws213fb_write,
	.fb_fillrect	= ws213fb_fillrect,
	.fb_copyarea	= ws213fb_copyarea,
	.fb_imageblit	= ws213fb_imageblit,
};


static void ws213fb_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	ws213fb_update_display(info->par);
}

static struct fb_deferred_io ws213fb_defio = {
	.delay		= HZ/30,
	.deferred_io	= ws213fb_deferred_io,
};


static int ws213fb_spi_probe(struct spi_device *spi)
{
	struct fb_info *info;
	int retval = -ENOMEM;

	struct ws213fb_platform_data *pdata = spi->dev.platform_data;

	int vmem_size = width*height*bpp/8;
	u8 *vmem;
	struct ws213fb_par *par;

	
	vmem = vmalloc(vmem_size);
	if (!vmem)
		return retval;

	info =framebuffer_alloc(sizeof(struct ws213fb_par),&spi ->dev);
	if (!info)
	{
		goto fballoc_fail;
	}
	
	info->screen_base = (u8 __force __iomem *)vmem;
	info->fbops = &ws213fb_ops;
	info->fix = ws213fb_fix;
	info->fix.smem_len = vmem_size;
	info->var = ws213fb_var;

	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->var.transp.offset = 0;
	info->var.transp.length = 0;
	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;
	
	info->fbdefio = &ws213fb_defio;
	fb_deferred_io_init(info);

	par = info->par;
	par->info = info;
	par->spi = spi;
	par->rst = pdata->rst_gpio;
	par->dc = pdata->dc_gpio;
	par->busy = pdata->busy_gpio;

	
#ifdef __LITTLE_ENDIAN
	vmem = vzalloc(vmem_size);
	if (!vmem)
		return retval;
	par->ssbuf = vmem;
#endif

	retval = register_framebuffer(info);
	if (retval < 0)
		goto fbreg_fail;


	spi_set_drvdata(spi, info);

	retval = ws213fb_init_display(par);
	if (retval < 0)
		goto init_fail;

	// dev_dbg("fb%d: %s frame buffer device,\n\tusing %d KiB of video memory\n",
	// 	info->node, info->fix.id, vmem_size);

	// dev_dbg("fb is created");

	return 0;

init_fail:
	spi_set_drvdata(spi, NULL);

fbreg_fail:
	framebuffer_release(info);

fballoc_fail:
	vfree(vmem);

	return retval;
}


static void ws213fb_spi_remove(struct spi_device *spi)
{
	struct fb_info *p =spi_get_drvdata(spi);
	unregister_framebuffer(p);
	fb_dealloc_cmap(&p->cmap);
	iounmap(p->screen_base);
	framebuffer_release(p);
}


enum waveshare_devices{
	DEV_WS_213,
	DEV_WS_27,
};

struct waveshare_eink_device_properties{
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
};

static struct waveshare_eink_device_properties devices[] = 
{
	[DEV_WS_213] = {.width = 128, .height =250, .bpp = 1},
	[DEV_WS_27]  = {.width =176, .height = 264, .bpp =1},
};

static struct spi_device_id ws213fb_spi_tbl[]={
	{"waveshare_213",(kernel_ulong_t)&devices[DEV_WS_213]},
	{"waveshare_27",(kernel_ulong_t)&devices[DEV_WS_27]},
	{ },
};


MODULE_DEVICE_TABLE(spi,ws213fb_spi_tbl);


static struct spi_driver ws213fb_driver={
	.driver={
		.name 		=	"waveshare_fb_spi",
		.owner 		=THIS_MODULE,
	},
	
	.id_table	=	ws213fb_spi_tbl,
	.probe		=	ws213fb_spi_probe,
	.remove 	=	ws213fb_spi_remove, 
};




static struct spi_device_id waveshare_eink_tbl[] = {
	{"waveshare_213",(kernel_ulong_t)&devices[DEV_WS_213] },
	{"waveshare_27",(kernel_ulong_t)&devices[DEV_WS_27]},
	{ },
};

static int __init ws213fb_init(void)
{
	return spi_register_driver(&ws213fb_driver);
}

static void __exit ws213fb_exit(void)
{
	printk("remove fb driver\n");
	spi_unregister_driver(&ws213fb_driver);
}


module_init(ws213fb_init);
module_exit(ws213fb_exit);

MODULE_ALIAS("platform:eink");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thong Nguyen");
MODULE_DESCRIPTION("FB Display driver");
MODULE_VERSION("0.1");
