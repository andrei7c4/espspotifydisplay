#ifndef HWCONF_H_
#define HWCONF_H_

//#define WEMOS_NODEMCU_OLED_BOARD

#ifdef WEMOS_NODEMCU_OLED_BOARD
#define DISP_TYPE		1106
#define BUTTON_ON_GPIO0
#else
#define DISP_TYPE		1322
#define BUTTONS_ON_ADC
#define MPU_ON_SPI
#endif


#endif /* HWCONF_H_ */
