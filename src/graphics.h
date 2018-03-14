#ifndef INCLUDE_DISPLAY_H_
#define INCLUDE_DISPLAY_H_

#include "typedefs.h"


#define TITLE_OFFSET	0
#define TITLE_HEIGHT	20

#define ARTIST_OFFSET	20
#define ARTIST_HEIGHT	20

#define PROGBAR_OFFSET	53
#define PROGBAR_HEIGHT	(DISP_HEIGHT-PROGBAR_OFFSET)

#define BLANK_SPACE_OFFSET	(ARTIST_OFFSET+ARTIST_HEIGHT)
#define BLANK_SPACE_HEIGHT	(PROGBAR_OFFSET-BLANK_SPACE_OFFSET)


struct GfxBuf_
{
	uchar *buf;
	int memWidth;
	int width;
	int height;
};
typedef struct GfxBuf_ GfxBuf;

struct Label_
{
	GfxBuf buf;
	int offset;
	int scrollEn;
	int vScrollY;
	int hScrollX;
	int hScrollBit;
};
typedef struct Label_ Label;

extern GfxBuf MainGfxBuf;
extern GfxBuf TempGfxBuf;
extern GfxBuf *activeBuf;

extern Label TitleLabel;
extern Label ArtistLabel;

extern int inverseColor;


void GfxBufAlloc(GfxBuf *buf, int width);
void GfxBufCopy(GfxBuf *dst, GfxBuf *src, int dstRow);

void activeBufFill(uchar data, int row, int height);
void activeBufClearAll(void);
void activeBufClearProgBar(void);



void drawImage(int x, int y, const uint *image);
void drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize);

void drawPixel(int x, int y, int color);
void drawLine(int x0, int y0, int x1, int y1, int color);
void drawRect(int x0, int y0, int x1, int y1, int color);


#endif /* INCLUDE_DISPLAY_H_ */
