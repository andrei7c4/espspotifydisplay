#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

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

typedef enum{
	Page0 = 0,
	Page1
}DispPage;

void dispUpdate(DispPage page);

extern int dispScrollCurLine;

void dispDimmingStart(void);
void dispVerticalSqueezeStart(void);
void dispUndimmStart(void);

void scrollDisplay(void);

void dispSetOrientation(Orientation orientation);


#endif /* SRC_DISPLAY_H_ */
