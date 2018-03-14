#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

#include "hwconf.h"

#if DISP_TYPE == 1322
#define DISP_WIDTH		256
#elif DISP_TYPE == 1106
#define DISP_WIDTH		128
#else
#error "Only SSD1322 and SH1106 based displays are supported"
#endif

#define DISP_HEIGHT		64
#define DISP_MEMWIDTH	(DISP_WIDTH/8)

#define CONTRAST_LEVEL_INIT		0


typedef enum{
	stateOff,
	stateDimmed,
	stateOn,
}DispState;
extern DispState displayState;

typedef enum {
	orient0deg,
	orient180deg,
}Orientation;
extern Orientation dispOrient;


void dispUpdate(int row, int height);
void dispUpdateFull(void);
void dispUpdateProgBar(void);


void dispDimmingStart(void);
void dispSmoothTurnOff(void);
void dispUndimmStart(void);

void dispSetOrientation(Orientation orientation);


#endif /* SRC_DISPLAY_H_ */
