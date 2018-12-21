#include <linux/module.h>
#include <linux/spi/spi.h>
#include "fbws.h"

#define SPI_BUS 	0
#define SPI_BUS_CS1 	0
#define SPI_BUS_SPEED 	960000
//#define SPI_BUS_SPEED 	2000000

const char this_driver_name[] = "waveshare213";

static struct ourfb_platform_data ourfb_data = {
       .rst_gpio       = 54, //IOT0_GPI03- GPIOPIN3
       //.dc_gpio        = 29, //IOT0_GPIO4- GPIOPIN4
       .dc_gpio        = 49, //RPI GPIO 22 - Pin 11

       .busy_gpio	=	61, //GPIO pin 7
};


static int __init add_ourfb_device_to_bus(void)
{
	struct spi_master *spi_master;
	struct spi_device *spi_device;
	struct device *pdev;
	char buff[64];
	int status = 0;

	printk("add_ourfb_device_to_bus is called\n");

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
	/* specify a chip select line */
	spi_device->chip_select = SPI_BUS_CS1;

	spi_device->max_speed_hz =SPI_BUS_SPEED;
	spi_device->mode =SPI_MODE_0;
	spi_device->bits_per_word = 8;
	spi_device->irq = -1;
	//spi_device->controller_state = NULL;
	//spi_device->controller_data = NULL;

	printk("GET SPI Device\n");

	/* Check whether this SPI bus.cs is already claimed */
	snprintf(buff, sizeof(buff), "%s.%u", 
			dev_name(&spi_device->master->dev),
			spi_device->chip_select);

	printk(buff);
	printk("bus_find_device_by_name is called\n");
	pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);


 	if (pdev) {

 		printk("have pdev\n");
		/* We are not going to use this spi_device, so free it */ 
		spi_dev_put(spi_device);
		
		/* 
		 * There is already a device configured for this bus.cs combination.
		 * It's okay if it's us. This happens if we previously loaded then 

                 * unloaded our driver. 
                 * If it is not us, we complain and fail.
		 */
		if (pdev->driver && pdev->driver->name && 
				strcmp(this_driver_name, pdev->driver->name)) {
			printk("device is register\n");
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
module_init(add_ourfb_device_to_bus);

static void __exit rpi_ourfb_exit(void)
{
}
module_exit(rpi_ourfb_exit);

MODULE_AUTHOR("Neil Greatorex");
MODULE_DESCRIPTION("Bind SPI to ourfb");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");


