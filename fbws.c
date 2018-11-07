#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/fb.h>
#include <linux/spi/spi.h>


#include "fbws.h"

static struct spi_device_id ourfb_spi_tbl[]={
	{"waveshare",0},
	{ },
};



//Old source from pci
// static struct spi_device_id ourfb_spi_tbl[]={
// 	{SPI_VENDOR_ID_123,SPI_DEVICE_ID_123,SPI_ANY_ID,SPI_ANY_ID},
// 	{0}
// };

static int ourfb_spi_init(struct spi_device *spi)
{
	struct fb_info *info;
	info =framebuffer_alloc(sizeof(struct ourfb_par),&spi ->dev);
	register_framebuffer(info);
	return 0;

}

static void ourfb_spi_remove(struct spi_device *spi)
{
	struct fb_info *p =spi_get_drvdata(spi);
	unregister_framebuffer(p);
	fb_dealloc_cmap(&p->cmap);
	iounmap(p->screen_base);
	framebuffer_release(p);

}


static struct spi_driver ourfb_driver={
	.driver={
		.name 		=	"waveshare",
		.owner 		=THIS_MODULE,

	},
	
	.id_table	=	ourfb_spi_tbl,
	.probe		=	ourfb_spi_init,
	.remove 	=	ourfb_spi_remove, 
};


static int __init ourfb_init(void)
{
	return spi_register_driver(&ourfb_driver);
}

static void __exit ourfb_exit(void)
{

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
