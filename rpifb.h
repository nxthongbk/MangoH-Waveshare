#ifndef FBWS_H
#define FBWS_H



#define DRVNAME		"ws213"
#define WIDTH		128
#define HEIGHT		160
#define BPP		16

struct our_function {
	u16 cmd;
	u16 data;
};


/* Init script function */
struct ourfb_function {
	u16 cmd;
	u16 data;
};

/* Init script commands */
enum ourfb_cmd {
	WS_START,
	WS_END,
	WS_CMD,
	WS_DATA,
	WS_DELAY
};

struct ourfb_par{
	struct spi_device *spi;
	struct fb_info *info;
	u16 *ssbuf;
	int rst;
	int dc;
};

struct ourfb_platform_data {
	int rst_gpio;
	int dc_gpio;
};

/* WS Commands */
#define WS_NOP	0x0
#define WS_SWRESET	0x01
#define WS_RDDID	0x04
#define WS_RDDST	0x09
#define WS_SLPIN	0x10
#define WS_SLPOUT	0x11
#define WS_PTLON	0x12
#define WS_NORON	0x13
#define WS_INVOFF	0x20
#define WS_INVON	0x21
#define WS_DISPOFF	0x28
#define WS_DISPON	0x29
#define WS_CASET	0x2A
#define WS_RASET	0x2B
#define WS_RAMWR	0x2C
#define WS_RAMRD	0x2E
#define WS_COLMOD	0x3A
#define WS_MADCTL	0x36
#define WS_FRMCTR1	0xB1
#define WS_FRMCTR2	0xB2
#define WS_FRMCTR3	0xB3
#define WS_INVCTR	0xB4
#define WS_DISSET5	0xB6
#define WS_PWCTR1	0xC0
#define WS_PWCTR2	0xC1
#define WS_PWCTR3	0xC2
#define WS_PWCTR4	0xC3
#define WS_PWCTR5	0xC4
#define WS_VMCTR1	0xC5
#define WS_RDID1	0xDA
#define WS_RDID2	0xDB
#define WS_RDID3	0xDC
#define WS_RDID4	0xDD
#define WS_GMCTRP1	0xE0
#define WS_GMCTRN1	0xE1
#define WS_PWCTR6	0xFC


#endif /* FBWS_H */
