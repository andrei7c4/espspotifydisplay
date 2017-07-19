#ifndef INCLUDE_DISPLAY_H_
#define INCLUDE_DISPLAY_H_

#include "typedefs.h"
#include "fonts.h"

#define DISP_HEIGHT		64
#define DISP_WIDTH		256
#define DISP_MEMWIDTH	(DISP_WIDTH/8)

#define TITLE_HEIGHT	13

extern uchar mem[DISP_HEIGHT][DISP_MEMWIDTH];
extern uchar mem2[TITLE_HEIGHT][DISP_MEMWIDTH];
extern int memHeight;

typedef enum{
	MainMemBuf,
	SecondaryMemBuf
}MemBufType;


void dispSetActiveMemBuf(MemBufType memBuf);
void dispCopySecMemBufToMain(void);
int dispTitleScrollStep(int reset);
void dispFillMem(uchar data, int lines);

void drawImage(int x, int y, const uint *image);
void drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize);

void inverseColor(int inverse);

void (*drawPixel)(int x, int y, char color);
void drawPixelNormal(int x, int y, char color);
void drawPixelInverse(int x, int y, char color);
void drawLine(int x0, int y0, int x1, int y1, char color);
void drawRect(int x0, int y0, int x1, int y1, char color);


#endif /* INCLUDE_DISPLAY_H_ */
