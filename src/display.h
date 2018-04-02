#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

#include "hwconf.h"

// resolutions commonly associated with supported display controllers
#if DISP_TYPE == 1322
#define DISP_WIDTH		256
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
#define DISP_WIDTH		128
#else
#define DISP_WIDTH		0
#endif

#if DISP_TYPE == 1322 || DISP_TYPE == 1106
#define DISP_HEIGHT		64
#elif DISP_TYPE == 1306
#define DISP_HEIGHT		32
#else
#define DISP_HEIGHT		0
#endif

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
void dispUpdateLabels(int title, int artist, int album);
void dispUpdateProgBar(void);
void dispClearBlankSpace(int showAlbum);


void dispDimmingStart(void);
void dispSmoothTurnOff(void);
void dispUndimmStart(void);

void dispSetOrientation(Orientation orientation);


#endif /* SRC_DISPLAY_H_ */
