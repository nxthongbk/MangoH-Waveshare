#ifndef FBWS_H
#define FBWS_H



#define DRVNAME		"ws213"
#define WIDTH		128
#define HEIGHT		250
//#define HEIGHT		250
#define BPP			1

struct our_function {
	u8 cmd;
	u8 data;
};


/* Init script function */
struct ourfb_function {
	u8 cmd;
	u8 data;
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
	u8 *ssbuf;
	int rst;
	int dc;
	int busy;
};

struct ourfb_platform_data {
	int rst_gpio;
	int dc_gpio;
	int busy_gpio;
};

/* WS Commands */
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
			

#endif /* FBWS_H */
