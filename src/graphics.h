#ifndef INCLUDE_DISPLAY_H_
#define INCLUDE_DISPLAY_H_

#include "typedefs.h"
#include "display.h"
#include "fonts.h"

#if DISP_HEIGHT >= 64
#define PROGBAR_OFFSET	53
#elif DISP_HEIGHT == 32
#define PROGBAR_OFFSET	31
#elif DISP_HEIGHT == 0
#define PROGBAR_OFFSET	0
#endif

#define PROGBAR_HEIGHT	(DISP_HEIGHT-PROGBAR_OFFSET)

extern int inverseColor;

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
	int scrollInt;
	int vScrollY;
	int hScrollX;
	int hScrollBit;
	const Font *font;
};
typedef struct Label_ Label;

extern GfxBuf MainGfxBuf;
extern GfxBuf TempGfxBuf;
extern GfxBuf *activeBuf;

extern Label TitleLabel;
extern Label ArtistLabel;
extern Label AlbumLabel;

void setLabelDimensions(int showAlbum);

void GfxBufAlloc(GfxBuf *buf, int width);
void GfxBufCopy(GfxBuf *dst, GfxBuf *src, int dstRow);

void activeBufFill(uchar data, int row, int height);
void activeBufClear(int row, int height);
void activeBufClearAll(void);
void activeBufClearProgBar(void);



void drawImage(int x, int y, const uint *image);
void drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize);

void drawPixel(int x, int y, int color);
void drawLine(int x0, int y0, int x1, int y1, int color);
void drawRect(int x0, int y0, int x1, int y1, int color);


#endif /* INCLUDE_DISPLAY_H_ */
