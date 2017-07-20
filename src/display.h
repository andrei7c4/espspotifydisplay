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


void dispUpdate(int row, int height);
void dispUpdateFull(void);

void scrollTitle(void);
void scrollTitleArtist(void);


extern int dispScrollCurLine;

void dispDimmingStart(void);
void dispVerticalSqueezeStart(void);
void dispUndimmStart(void);

void dispSetOrientation(Orientation orientation);


#endif /* SRC_DISPLAY_H_ */
