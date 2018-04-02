#ifndef HWCONF_H_
#define HWCONF_H_

//#define WEMOS_NODEMCU_OLED_BOARD

#ifdef WEMOS_NODEMCU_OLED_BOARD

#define DISP_TYPE		1106
#define BUTTON_ON_GPIO0

#else	// custom hardware

#define DISP_TYPE		1322
//#define DISP_TYPE		1306
//#define DISP_TYPE		0

#define BUTTONS_ON_ADC
//#define BUTTON_ON_GPIO0
#define MPU_ON_SPI

#endif


#if DISP_TYPE != 1322 && DISP_TYPE != 1306 && DISP_TYPE != 1106 && DISP_TYPE != 0
#error "Only SSD1322, SSD1306 and SH1106 based displays are supported"
#endif


#endif /* HWCONF_H_ */
