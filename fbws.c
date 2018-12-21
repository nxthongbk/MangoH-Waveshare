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

#include "fbws.h"

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
	// u8 *ssbuf = par->ssbuf;
 	// ssbuf[0] = data;
 	// return spi_write(par->spi, ssbuf, 2);

	u8 txbuf[2];
	txbuf[0] = data;
	return spi_write(par->spi,&txbuf[0],1);
}

static void our_write_data(struct ourfb_par *par, u8 data)
{
	int ret = 0;
	/* Set data mode */
	gpio_set_value(par->dc, 1);
	
	ret = our_write(par, data);
	if (ret < 0)
	{
		printk("spi write data error\n");
		pr_err("%s: write data %02x failed with status %d\n",
			par->info->fix.id, data, ret);
	}
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

/**
*	@brief: wait until the busy pin goes LOW
*/
static void wait_until_idle(struct ourfb_par *par)
{
	//wait Busy pin to LOW level
	while(gpio_get_value(par->busy) != 0)
	{
		mdelay(100);
	}
}

/**
*	@brief: set address for data R/W
*/
static void our_set_addr_win(struct ourfb_par *par,
				int xs, int ys, int xe, int ye)
{
	//set address_win
	our_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	our_write_data(par, 0x00);
	our_write_data(par, xs+2);
	our_write_data(par, 0x00);
	our_write_data(par, xe+2);

	our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION);
	our_write_data(par, 0x00);
	our_write_data(par, ys+1);
	our_write_data(par, 0x00);
	our_write_data(par, ye+1);
}

/*
static void our_run_cfg_script(struct ourfb_par *par)
{
	printk("our_run_cfg_script is called\n");

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
*/
/**
/*	@brief: Reset display
*/
static void our_reset(struct ourfb_par *par)
{
	/* Reset controller */
	gpio_set_value(par->rst, 0);
	mdelay(200);
	gpio_set_value(par->rst, 1);
	mdelay(200);
}


/**
/*	@brief: Set Lut full update or partial update
*/
static void set_lut(struct ourfb_par *par,unsigned char* lut)
{
	 our_write_cmd(par,WS_WRITE_LUT_REGISTER);
	 int i;
	 for(i = 0;i < 30; i++)
	 {
 		our_write_data(par,lut[i]);
	 }
}

/**
/*	@brief: Init Lut
*/
static int int_lut(struct ourfb_par *par,unsigned char* lut)
{
	
	our_reset(par);

    our_write_cmd(par,WS_DRIVER_OUTPUT_CONTROL);
    our_write_data(par,(HEIGHT - 1) & 0xFF);
    our_write_data(par,((HEIGHT - 1) >> 8) & 0xFF);
    our_write_data(par,0x00);                     // GD = 0; SM = 0; TB = 0;
    our_write_cmd(par,WS_BOOSTER_SOFT_START_CONTROL);
    our_write_data(par,0xD7);
    our_write_data(par,0xD6);
    our_write_data(par,0x9D);
    our_write_cmd(par,WS_WRITE_VCOM_REGISTER);
    our_write_data(par,0xA8);                     // VCOM 7C
    our_write_cmd(par,WS_SET_DUMMY_LINE_PERIOD);
    our_write_data(par,0x1A);                     // 4 dummy lines per gate
    our_write_cmd(par,WS_SET_GATE_TIME);
    our_write_data(par,0x08);                     // 2us per line
    our_write_cmd(par,WS_DATA_ENTRY_MODE_SETTING);
    our_write_data(par,0x03);                     // X increment; Y increment
    set_lut(par,lut);
   
    return 0;
}


/**
/*	@brief: Set Memory Area
*/
static void set_memory_area(struct ourfb_par *par,int x_start,int y_start,int x_end,int y_end)
{
	our_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	our_write_data(par, (x_start >> 3) & 0xFF);
	our_write_data(par, (x_end >> 3) & 0xFF);

	our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION);
	our_write_data(par,y_start & 0xFF);
	our_write_data(par, (y_start >> 8) & 0xFF);
	our_write_data(par,y_end & 0xFF);
	our_write_data(par, (y_end >> 8) & 0xFF);

}


/**
/*	@brief: Set Memory Pointer
*/
static void set_memory_pointer(struct ourfb_par *par,int x, int y)
{
	our_write_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER);
	our_write_data(par, (x >> 3) & 0xFF);

	our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER);
	our_write_data(par, y & 0xFF);
	our_write_data(par, (y >> 8) & 0xFF);
	wait_until_idle(par);
}

/**
/*	@brief: Clear Frame Memory
*/
static void clear_frame_memory(struct ourfb_par *par,unsigned char color)
{
	printk("call clear_frame_memory\n");
	set_memory_area(par,0,0,WIDTH-1,HEIGHT-1);
	int j;
	for(j = 0; j < HEIGHT ;j++)
	{
		set_memory_pointer(par,0 , j);
		our_write_cmd(par, WS_WRITE_RAM);
		int i;
		for(i = 0; i < WIDTH/8 ;i++)
		{
			our_write_data(par,color);			
		}
	}
}

/**
/*	@brief: Set frame memory to update screen
*/
static void set_frame_memory(struct ourfb_par *par,unsigned char* image_buffer)
{
	set_memory_area(par, 0, 0, WIDTH-1, HEIGHT-1);
	

	int j;
	for(j = 0; j < HEIGHT ; j++)
	{
		set_memory_pointer(par,0 , j);
		our_write_cmd(par, WS_WRITE_RAM);

		int i;
		for(i = 0; i < (WIDTH-1)/8 ; i++)
		{
			our_write_data(par, image_buffer[i + j * (WIDTH/8)]);
		}
	}
}

/**
/*	@brief: Display frame 
*/
static void display_frame(struct ourfb_par *par)
{
	our_write_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2);
	our_write_data(par, 0xC4);
	our_write_cmd(par, WS_MASTER_ACTIVATION);
	our_write_cmd(par, WS_TERMINATE_FRAME_READ_WRITE);
	
	wait_until_idle(par);
}

/**
/*	@brief: Init display when module is loaded
*/
static int ourfb_init_display(struct ourfb_par *par)
{
	printk("ourfb_init_display is called\n");

	/* TODO: Need some error checking on gpios */
	gpio_request(par->rst, "sysfs"); 
	gpio_request(par->dc, "sysfs");
	gpio_request(par->busy, "sysfs");
	        
    gpio_direction_output(par->dc, true);   // Set the gpio to be in output mode and on
   	gpio_set_value(par->dc, 0);          // Not required as set by line above (here for reference)
   	gpio_export(par->dc, true);

   	gpio_direction_output(par->rst, true);   // Set the gpio to be in output mode and on
   	gpio_set_value(par->rst, 0);          // Not required as set by line above (here for reference)
   	gpio_export(par->rst, true);


   	gpio_direction_input(par->busy);   // Set the gpio to be in output mode and on
   	gpio_set_value(par->busy, 0);          // Not required as set by line above (here for reference)
   	gpio_export(par->busy, true);


    if(int_lut(par,lut_full_update)!=0)
	 {
	 	printk("Init lut error");
	 } 
		
	//clear screen	
	clear_frame_memory(par,0xFF);
	display_frame(par);

	return 0;
}

/**
/*	@brief: Update screen when mmap from user space
*/
static void ourfb_update_display(struct ourfb_par *par)
{
	
	printk("ourfb_update_display is called\n");

	int ret = 0;
	u8 *vmem = par->info->screen_base;
#ifdef __LITTLE_ENDIAN
	printk("goto __LITTLE_ENDIAN\n");
 	int i;
	u8 *vmem8 = (u8 *)vmem;
	//u16 *ssbuf = par->ssbuf;
 	u8 *ssbuf = par->ssbuf;

 	for (i=0; i<WIDTH*HEIGHT*BPP/8; i++)
 	{
 		ssbuf[i] = vmem8[i];
 	}

 	set_frame_memory(par,ssbuf);
 	display_frame(par);

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
 // 	printk("write command121\n");
	// our_write_cmd(par, 0x00);
	// our_write_cmd(par, 0x01);
	// our_write_cmd(par, 0x02);
	// our_write_cmd(par, 0x03);
	// our_write_cmd(par, 0x04);
	// our_write_cmd(par,0x05);

	//printk("write command----------\n");
	//our_write_data_buf(par, (u8 *)ssbuf,WIDTH*HEIGHT*BPP/8);

	
	

	// // our_set_addr_win(par,0,0,WIDTH-1, HEIGHT-1);
	// our_set_addr_win(par, 0, 0, WIDTH-1, HEIGHT-1);

	/* Internal RAM write command */
	//our_write_cmd(par, WS_RAMWR);

	

	/* Blast framebuffer to ST7735 internal display RAM */
// #ifdef __LITTLE_ENDIAN
// 	printk("goto bellow __LITTLE_ENDIAN");
// 	ret = our_write_data_buf(par, (u8 *)ssbuf, WIDTH*HEIGHT*BPP/8);
// #else
// 	printk("goto not  __LITTLE_ENDIAN");
// 	ret = our_write_data_buf(par, vmem, WIDTH*HEIGHT*BPP/8);
// #endif
// 	if (ret < 0)
// 		pr_err("%s: spi_write failed to update display buffer\n",
// 			par->info->fix.id);
}


void ourfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	printk("ourfb_fillrect is called\n");

	struct ourfb_par *par = info->par;

	sys_fillrect(info, rect);

	ourfb_update_display(par);
}

void ourfb_copyarea(struct fb_info *info, const struct fb_copyarea *area) 
{
	printk("ourfb_copyarea is called\n");
	struct ourfb_par *par = info->par;

	sys_copyarea(info, area);

	ourfb_update_display(par);
}

void ourfb_imageblit(struct fb_info *info, const struct fb_image *image) 
{
	printk("ourfb_imageblit is called\n");

	struct ourfb_par *par = info->par;

	sys_imageblit(info, image);

	ourfb_update_display(par);
}

/**
/*	@brief: udpate when get write action from user space
*/
static ssize_t ourfb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	//ourfb_update_display1(par);

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

/**
/*	@brief: fb operator define
*/
static struct fb_ops ourfb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	=fb_sys_read,
	.fb_write	= ourfb_write,
	.fb_fillrect	= ourfb_fillrect,
	.fb_copyarea	= ourfb_copyarea,
	.fb_imageblit	= ourfb_imageblit,

	//For default fb operator
	// .fb_fillrect	= cfb_fillrect,
	// .fb_copyarea	= cfb_copyarea,
	// .fb_imageblit	= cfb_imageblit,
};

//Config Deferred IO
static void ourfb_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	ourfb_update_display(info->par);
}
static struct fb_deferred_io ourfb_defio = {
	.delay		= HZ,
	.deferred_io	= ourfb_deferred_io,
};

static void ourfb_deferred_io(struct fb_info *info,	struct list_head *pagelist)
 {
 	ourfb_update_display(info->par);
 } 

/**
/*	@brief: Init FB SPI
*/
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

	vmem = vmalloc(vmem_size);
	if (!vmem)
		return retval;
	
	//vmem = vzalloc(vmem_size);
	//if (!vmem)
	//	return retval;

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
	//For color display (TBD)
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
	
	info->fbdefio = &ourfb_defio;
	fb_deferred_io_init(info);

	par = info->par;
	par->info = info;
	par->spi = spi;
	par->rst = pdata->rst_gpio;
	par->dc = pdata->dc_gpio;
	par->busy = pdata->busy_gpio;

	
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

	printk("fb is created");

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


module_init(ourfb_init);
module_exit(ourfb_exit);

MODULE_ALIAS("platform:eink");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thong Nguyen");
MODULE_DESCRIPTION("FB Display driver");
MODULE_VERSION("0.1");
