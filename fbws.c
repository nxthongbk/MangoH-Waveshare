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
	u8 *ssbuf = par->ssbuf;
 	ssbuf[0] = data;

 	return spi_write(par->spi, ssbuf, 2);
}

static void our_write_data(struct ourfb_par *par, u8 data)
{
	int ret = 0;
	/* Set data mode */

	
	gpio_set_value(par->dc, 1);
	//udelay(100);
	//printk("write data: %02x\n",data);

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
	//printk("DC=0 write command: %02x\n",data);

	ret = our_write(par, data);
	if (ret < 0)
		pr_err("%s: write command %02x failed with status %d\n",
			par->info->fix.id, data, ret);
	
}

/**
*	@brief: wait until the busy pin goes LOW
*/
static void wait_until_idle(void)
{
	//wait BUSY PIN HERE TBD
	udelay(10);
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


//Reset display
static void our_reset(struct ourfb_par *par)
{
	/* Reset controller */
	gpio_set_value(par->rst, 0);
	mdelay(200);
	gpio_set_value(par->rst, 1);
	mdelay(200);
}


//Update screen when mmap from user space
//status: developing
static void ourfb_update_display(struct ourfb_par *par)
{
	
	printk("ourfb_update_display is called\n");

	int ret = 0;
	u8 *vmem = par->info->screen_base;
#ifdef __LITTLE_ENDIAN
	printk("goto __LITTLE_ENDIAN\n");
 	int i;
	u16 *vmem16 = (u16 *)vmem;
	//u16 *ssbuf = par->ssbuf;
 	u8 *ssbuf = par->ssbuf;


 	for (i=0; i<WIDTH*HEIGHT*BPP/8; i++)
 	{
 		//ssbuf[i] = swab16(vmem16[i]);
 		ssbuf[i] = 0xFF;
 	}

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


static void set_lut(struct ourfb_par *par,unsigned char* lut)
{
	printk("call set lut\n");
	 our_write_cmd(par,WS_WRITE_LUT_REGISTER);
	 int i;
	 for(i = 0;i < 30; i++)
	 {
 		our_write_data(par,lut[i]);
	 }
}


//Init for e-ink display
static int int_lut(struct ourfb_par *par,unsigned char* lut)
{
	
	printk("Call init lut\n");
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
    /* EPD hardware init end */
    return 0;
}


//Set memory area
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


//Set memeory pointer
static void set_memory_pointer(struct ourfb_par *par,int x, int y)
{
	our_write_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER);
	our_write_data(par, (x >> 3) & 0xFF);

	our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER);
	our_write_data(par, y & 0xFF);
	our_write_data(par, (y >> 8) & 0xFF);
	wait_until_idle();
}


//Clear frame memory by seting color
static void clear_frame_memory(struct ourfb_par *par,unsigned char color)
{
	printk("call clear_frame_memory\n");
	set_memory_area(par,0,0,WIDTH-1,HEIGHT-1);
	int j;
	for(j = 0; j < HEIGHT ;j++)
	{
		set_memory_pointer(par,0 , j);
		our_write_cmd(par, WS_RAMWR);
		int i;
		for(i = 0; i < WIDTH/8 ;i++)
		{
			//set color
			our_write_data(par,color);
			//printk("set %d,%d\n",i,j);
		}
	}
}

static void set_frame_memory(void)
{
	//Will be developer to diplay image
}

static void display_frame(struct ourfb_par *par)
{
	our_write_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2);
	our_write_data(par, 0xC4);
	our_write_cmd(par, WS_MASTER_ACTIVATION);
	our_write_cmd(par, WS_TERMINATE_FRAME_READ_WRITE);
	
	wait_until_idle();
}


//For testing update display
static void ourfb_update_display1(struct ourfb_par *par)
{
	//Set memory Area
	printk("Set memory Area\n");
	int x_start = 0;
	int y_start = 0;
	int x_end = WIDTH-1;
	int y_end = HEIGHT-1;

	our_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	our_write_data(par, (x_start >> 3) & 0xFF);
	our_write_data(par, (x_end >> 3) & 0xFF);

	our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION);
	our_write_data(par,y_start & 0xFF);
	our_write_data(par, (y_start >> 8) & 0xFF);
	our_write_data(par,y_end & 0xFF);
	our_write_data(par, (y_end >> 8) & 0xFF);
	//End Set Memory Area
	
	//Start set frame memory line by ine
	printk("Start set frame memory line by line\n");
	int j=0;
	for (j = 0; j < HEIGHT; j++)
	{
		printk("j run time: %d\n",j);
		//Set memory Pointer 0 j
		our_write_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER);
		our_write_data(par, (0 >> 3) & 0xFF);

		our_write_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER);
		our_write_data(par, y_end & 0xFF);
		our_write_data(par, (j >> 8) & 0xFF);
		//Send command writeRAM
		our_write_cmd(par, WS_RAMWR);
		int i = 0;
		for(i = 0; i < WIDTH/8 ;i++)
		{
			//send color
			our_write_data(par,0xFF);
		}
	}
	//end set frame memory
	//start display Frame
	our_write_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2);
	our_write_data(par, 0xC4);
	our_write_cmd(par, WS_MASTER_ACTIVATION);
	our_write_cmd(par, WS_TERMINATE_FRAME_READ_WRITE);
	printk("End Display and out\n");
	wait_until_idle();
	
}


//Init display when module is loaded
static int ourfb_init_display(struct ourfb_par *par)
{
	printk("ourfb_init_display is called\n");

	/* TODO: Need some error checking on gpios */
	gpio_request(par->rst, "sysfs"); 
	gpio_request(par->dc, "sysfs");
	        
    gpio_direction_output(par->dc, true);   // Set the gpio to be in output mode and on
   	gpio_set_value(par->dc, 0);          // Not required as set by line above (here for reference)
   	gpio_export(par->dc, true);

   	gpio_direction_output(par->rst, true);   // Set the gpio to be in output mode and on
   	gpio_set_value(par->rst, 0);          // Not required as set by line above (here for reference)
   	gpio_export(par->rst, true);

	

	if(int_lut(par,lut_full_update)!=0)
	{
		printk("Init lut error");
	}	

	clear_frame_memory(par,0xFF);
	display_frame(par);

	return 0;
}

void ourfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	printk("ourfb_fillrect is called\n");

	struct ourfb_par *par = info->par;

	//sys_fillrect(info, rect);

	ourfb_update_display(par);
}

void ourfb_copyarea(struct fb_info *info, const struct fb_copyarea *area) 
{
	printk("ourfb_copyarea is called\n");
	struct ourfb_par *par = info->par;

	//sys_copyarea(info, area);

	ourfb_update_display(par);
}

void ourfb_imageblit(struct fb_info *info, const struct fb_image *image) 
{
	printk("ourfb_imageblit is called\n");

	struct ourfb_par *par = info->par;

	//sys_imageblit(info, image);

	ourfb_update_display(par);
}



static ssize_t ourfb_read(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	printk("ourfb_read is called\n");

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
	// printk("write from user space\n");
	// struct outfb_par *par = info->par;
	//ourfb_update_display(par);
	//return 0;
	
	

	


	// struct outfb_par *par = info->par;
	// our_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);


	// printk("start update display\n");
	// ourfb_update_display1(par);


	//return 0;
	// our_write_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION);
	// our_write_data(par, 0x00);
	// our_write_data(par, 0x01);
	// our_write_data(par, 0x02);
	// our_write_data(par, 0x03);

	// our_write_data(par, 0x04);
	// our_write_data(par, 0x05);
	// our_write_data(par, 0x06);
	// our_write_data(par, 0x07);

	// our_set_addr_win(par,0,0,WIDTH-1, HEIGHT-1);



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


static struct fb_ops ourfb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	=fb_sys_read,
	//.fb_read	= fb_sys_read,
	//.fb_read	= ourfb_read,
	.fb_write	= ourfb_write,
	// .fb_fillrect	= ourfb_fillrect,
	// .fb_copyarea	= ourfb_copyarea,
	// .fb_imageblit	= ourfb_imageblit,

	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


// static void ourfb_deferred_io(struct fb_info *info,
// 				struct list_head *pagelist)
// {
// 	ourfb_update_display(info->par);
// }
// static struct fb_deferred_io ourfb_defio = {
// 	.delay		= HZ,
// 	.deferred_io	= ourfb_deferred_io,
// };

// static void ourfb_deferred_io(struct fb_info *info,	struct list_head *pagelist)
//  {
//  	ourfb_update_display(info->par);
//  } 

// static struct fb_deferred_io ourfb_defio = {
// 	.delay		= HZ,
// 	.deferred_io	= ourfb_deferred_io,
// };

// static void hecubar_dpy__deferred_io(struct fb_info *info,	struct list_head *pagelist)
//  {
//  	ourfb_update_display(info->par);
//  } 

// static struct fb_deferred_io hecubafb_defio = {
// 	.delay		= HZ,
// 	.deferred_io	= hecubar_dpy__deferred_io,
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
	
	

	//Init FBIO
	// info->fbdefio = &hecubafb_defio;
	// fb_deferred_io_init(info);
	
	// info->fbdefio = &ourfb_defio;
	// fb_deferred_io_init(info);

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
MODULE_DESCRIPTION("E-Ink Display driver");
MODULE_VERSION("0.1");
