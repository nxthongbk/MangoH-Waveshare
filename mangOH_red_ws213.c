#include <linux/module.h>
#include <linux/spi/spi.h>
#include "fb_waveshare_213.h"

#define SPI_BUS 	32766
#define SPI_BUS_CS1 	1
#define SPI_BUS_SPEED 	960000

const char this_driver_name[] = "waveshare213";

static struct ws213fb_platform_data ourfb_data = {
       .rst_gpio       = 54,
       .dc_gpio        = 49,
       .busy_gpio	=	61, 
};


static int __init add_ws213fb_device_to_bus(void)
{
	struct spi_master *spi_master;
	struct spi_device *spi_device;
	struct device *pdev;
	char buff[64];
	int status = 0;


	spi_master = spi_busnum_to_master(SPI_BUS);
	if (!spi_master) {
		printk(KERN_ALERT "spi_busnum_to_master(%d) returned NULL\n",
			SPI_BUS);
		printk(KERN_ALERT "Missing modprobe omap2_mcspi?\n");
		return -1;
	}

	printk("spi_alloc_device\n");
	spi_device = spi_alloc_device(spi_master);
	if (!spi_device) {
		put_device(&spi_master->dev);
		printk(KERN_ALERT "spi_alloc_device() failed\n");
		return -1;
	}

	spi_device->chip_select = SPI_BUS_CS1;
	spi_device->max_speed_hz =SPI_BUS_SPEED;
	spi_device->mode =SPI_MODE_0;
	spi_device->bits_per_word = 8;
	spi_device->irq = -1;

	snprintf(buff, sizeof(buff), "%s.%u",dev_name(&spi_device->master->dev),
			spi_device->chip_select);

	pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);


 	if (pdev) {
		spi_dev_put(spi_device);
		
		if (pdev->driver && pdev->driver->name && 
				strcmp(this_driver_name, pdev->driver->name)) {
				printk(KERN_ALERT 
				"Driver [%s] already registered for %s\n",
				pdev->driver->name, buff);
			status = -1;
		} 
	} else {
		spi_device->dev.platform_data = &ourfb_data;
		spi_device->max_speed_hz = SPI_BUS_SPEED;
		spi_device->mode = SPI_MODE_3;
		spi_device->bits_per_word = 8;
		spi_device->irq = -1;

		spi_device->controller_state = NULL;
		spi_device->controller_data = NULL;
		strlcpy(spi_device->modalias, this_driver_name, SPI_NAME_SIZE);
		status = spi_add_device(spi_device);
		
		if (status < 0) {	
			spi_dev_put(spi_device);
			printk(KERN_ALERT "spi_add_device() failed: %d\n", 
				status);		
		}				
	}

	put_device(&spi_master->dev);

	return status;
}
module_init(add_ws213fb_device_to_bus);

static void __exit exit_ws213fb_device(void)
{

}
module_exit(exit_ws213fb_device);

MODULE_AUTHOR("Neil Greatorex");
MODULE_DESCRIPTION("Bind SPI to fb");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");


