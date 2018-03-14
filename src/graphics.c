#include <ets_sys.h>
#include <string.h>
#include <osapi.h>
#include <mem.h>
#include <limits.h>
#include "typedefs.h"
#include "common.h"
#include "conv.h"
#include "display.h"
#include "graphics.h"


LOCAL const uchar maskLut[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
LOCAL const uchar maskLutInv[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE};
int inverseColor = FALSE;

LOCAL uchar bufMain[DISP_HEIGHT*DISP_MEMWIDTH];
GfxBuf MainGfxBuf = {bufMain, DISP_MEMWIDTH, DISP_WIDTH, DISP_HEIGHT};

LOCAL uchar bufTemp[1*DISP_MEMWIDTH];
GfxBuf TempGfxBuf= {bufTemp, DISP_MEMWIDTH, DISP_WIDTH, 1};

GfxBuf *activeBuf = &MainGfxBuf;

Label TitleLabel = {{NULL, 0, 0, TITLE_HEIGHT}, TITLE_OFFSET, 0,0,0};
Label ArtistLabel = {{NULL, 0, 0, ARTIST_HEIGHT}, ARTIST_OFFSET, 0,0,0};

void ICACHE_FLASH_ATTR GfxBufAlloc(GfxBuf *buf, int width)
{
	width = roundUp(width, 8);
	if (width < DISP_WIDTH)
	{
		width = DISP_WIDTH;
	}
	else if (width > DISP_WIDTH)	// label will be scrolled
	{
		width += 32;			// add some blanking
	}

	int bufSize;
	if (buf->width != width)	// need to reallocate
	{
		os_free(buf->buf);

		buf->width = width;
		buf->memWidth = width / 8;
		bufSize = buf->memWidth * buf->height;
		if (bufSize > 4096)		// some sense
		{
			buf->memWidth = (4096 / buf->height) & -8;
			buf->width = buf->memWidth * 8;
			bufSize = buf->memWidth * buf->height;
		}

		buf->buf = (uchar*)os_malloc(bufSize);
		if (!buf->buf)
		{
			buf->width = 0;
			buf->memWidth = 0;
			return;
		}
	}
	else
	{
		bufSize = buf->memWidth * buf->height;
	}
	os_memset(buf->buf, inverseColor ? 0xFF : 0, bufSize);
}

void ICACHE_FLASH_ATTR GfxBufCopy(GfxBuf *dst, GfxBuf *src, int dstRow)
{
	uchar *dstBuf = dst->buf + (dst->memWidth*dstRow);
	uchar *srcBuf = src->buf;
	int row;
	for (row = 0; row < src->height && row < (dst->height - dstRow);
			row++, dstBuf += dst->memWidth, srcBuf += src->memWidth)
	{
		os_memcpy(dstBuf, srcBuf, dst->memWidth);
	}
}


void ICACHE_FLASH_ATTR activeBufFill(uchar data, int row, int height)
{
	uchar *dst = activeBuf->buf + row*activeBuf->memWidth;
    for (; row < activeBuf->height && height > 0; row++, height--, dst += activeBuf->memWidth)
    {
        os_memset(dst, data, activeBuf->memWidth);
    }
}

LOCAL void ICACHE_FLASH_ATTR activeBufClear(int row, int height)
{
	activeBufFill(inverseColor ? 0xFF : 0, row, height);
}

void ICACHE_FLASH_ATTR activeBufClearAll(void)
{
	activeBufClear(0, activeBuf->height);
}

void ICACHE_FLASH_ATTR activeBufClearProgBar(void)
{
	activeBufClear(BLANK_SPACE_OFFSET, BLANK_SPACE_HEIGHT+PROGBAR_HEIGHT);
}



LOCAL void ICACHE_FLASH_ATTR dispDrawBitmap(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize)
{
    int maxBmHeight = activeBuf->height - y;
    if (bmHeight > maxBmHeight)
    {
        bmHeight = maxBmHeight;
    }
    if (bmHeight <= 0)
        return;

    bmWidth /= 8;
    int memX = x/8;
    int bmWidthCpy = bmWidth;
    int maxBmWidth = activeBuf->memWidth - memX;
    if (bmWidthCpy > maxBmWidth)
    {
        bmWidthCpy = maxBmWidth;
    }
    if (bmWidthCpy <= 0)
        return;

    int i;
    uchar *dst = activeBuf->buf + (y*activeBuf->memWidth) + memX;
    if ((memX&3) == 0 && (bmWidthCpy&3) == 0)	// x and width multiple of 4 -> can access flash directly
    {
    	const uchar *pBitmap = (uchar*)bitmap;
		for (i = 0; i < bmHeight; i++, dst += activeBuf->memWidth)
		{
			os_memcpy(dst, pBitmap, bmWidthCpy);
			pBitmap += bmWidth;
		}
    }
    else	// x or width not multiple of 4 -> need a temp buffer to avoid alignment issues
    {
    	bitmapSize *= sizeof(uint);		// bitmapSize is dwords
        uchar *temp = (uchar*)os_malloc(bitmapSize);
        uchar *pTemp = temp;
        if (!temp)
        	return;
        os_memcpy(pTemp, bitmap, bitmapSize);
    	for (i = 0; i < bmHeight; i++, dst += activeBuf->memWidth)
    	{
    		os_memcpy(dst, pTemp, bmWidthCpy);
    		pTemp += bmWidth;
    	}
        os_free(temp);
    }
}

void ICACHE_FLASH_ATTR drawImage(int x, int y, const uint *image)
{
    int imgWidth = image[0];
    int imgHeight = image[1];
    int bitmapSize = image[2];
    dispDrawBitmap(x, y, imgWidth, imgHeight, image+4, bitmapSize);
}



LOCAL int ICACHE_FLASH_ATTR getPixel(int x, int y, int byteWidth, uchar *bitmap)
{
	uchar *pBuf = bitmap + y*byteWidth + x/8;
    return (*pBuf & maskLut[x&7]) != 0;
}

void ICACHE_FLASH_ATTR drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;

	int maxBmHeight = activeBuf->height - y;
    if (bmHeight > maxBmHeight)
    {
        bmHeight = maxBmHeight;
    }
    if (bmHeight <= 0)
        return;

    int byteWidth = roundUp(bmWidth,8)/8;

    int maxBmWidth = activeBuf->width - x;
    if (bmWidth > maxBmWidth)
    {
        bmWidth = maxBmWidth;
    }
    if (bmWidth <= 0)
        return;
	
	uchar *pBitmap = (uchar*)os_malloc(bitmapSize*4);
	if (!pBitmap)
		return;
	spiFlashRead(pBitmap, bitmap, bitmapSize*4);

	int bmX, bmY, dispX, dispY;
    for (bmX = 0, dispX = x; bmX < bmWidth; bmX++, dispX++)
    {
        for (bmY = 0, dispY = y; bmY < bmHeight; bmY++, dispY++)
        {
            drawPixel(dispX, dispY, getPixel(bmX, bmY, byteWidth, pBitmap));
        }
    }
	
	os_free(pBitmap);
}



void ICACHE_FLASH_ATTR drawPixel(int x, int y, int color)
{
    if (x >= activeBuf->width || y >= activeBuf->height)
    {
        return;
    }
    uchar *pBuf = activeBuf->buf + y*activeBuf->memWidth + x/8;
    x = x & 7;
    *pBuf = (*pBuf & maskLutInv[x]) | (-(color^inverseColor) & maskLut[x]);
}

void ICACHE_FLASH_ATTR drawLine(int x0, int y0, int x1, int y1, int color)
{
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = (dx>dy ? dx : -dy)/2, e2;

    for(;;)
    {
        drawPixel(x0,y0,color);
        if (x0==x1 && y0==y1) break;
        e2 = err;
        if (e2 >-dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

void ICACHE_FLASH_ATTR drawRect(int x0, int y0, int x1, int y1, int color)
{
    int x, y;
    int width = x1-x0+1;
    int height = y1-y0+1;
    if (width > height)
    {
        for (y = y0; y <= y1; y++)
        {
            drawLine(x0, y, x1, y, color);
        }
    }
    else
    {
        for (x = x0; x <= x1; x++)
        {
            drawLine(x, y0, x, y1, color);
        }
    }
}
