#ifndef INCLUDE_DISPLAY_H_
#define INCLUDE_DISPLAY_H_

#include "typedefs.h"
#include "fonts.h"

#define DISP_HEIGHT		64
#define DISP_WIDTH		256
#define DISP_MEMWIDTH	(DISP_WIDTH/8)

#define TITLE_OFFSET	0
#define TITLE_HEIGHT	20

#define ARTIST_OFFSET	20
#define ARTIST_HEIGHT	20


extern uchar mem1[DISP_HEIGHT][DISP_MEMWIDTH];
extern uchar (*pMem)[DISP_MEMWIDTH];
extern int memHeight;

typedef enum{
	MainMemBuf,
	TitleMemBuf,
	ArtistMemBuf,
	TempMemBuf
}MemBufType;

void dispSetActiveMemBuf(MemBufType memBuf);

void dispFillMem(uchar data, int row, int height);
void dispClearMem(int row, int height);
void dispClearMemAll(void);

void dispCopySecMemBufToMain(void);


void initTitleScroll(void);
void initArtistScroll(void);

int dispTitleScrollStep(void);
int dispArtistScrollStep(void);


void drawImage(int x, int y, const uint *image);
void drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize);

void inverseColor(int inverse);

void (*drawPixel)(int x, int y, char color);
void drawPixelNormal(int x, int y, char color);
void drawPixelInverse(int x, int y, char color);
void drawLine(int x0, int y0, int x1, int y1, char color);
void drawRect(int x0, int y0, int x1, int y1, char color);


#endif /* INCLUDE_DISPLAY_H_ */
