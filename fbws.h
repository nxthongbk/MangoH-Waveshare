#ifndef FBWS_H
#define FBWS_H

struct led_platform_data {
	int gpio;
};


struct ourfb_par{
	struct spi_device *spi;
	struct fb_info *info;
	u16 *ssbuf;
	int rst;
	int dc;
};
#endif /* FBWS_H */
