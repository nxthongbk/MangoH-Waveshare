#ifndef FB_WAVESHARE_213_H
#define FB_WAVESHARE_213_H

struct ws213fb_par{
	struct spi_device *spi;
	struct fb_info *info;
	u8 *ssbuf;
	int rst;
	int dc;
	int busy;
};

struct ws213fb_platform_data {
	int rst_gpio;
	int dc_gpio;
	int busy_gpio;
};

		
#endif /* FB_WAVESHARE_213_H */
