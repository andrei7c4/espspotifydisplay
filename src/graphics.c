#include <ets_sys.h>
#include <string.h>
#include <osapi.h>
#include <mem.h>
#include <limits.h>
#include "typedefs.h"
#include "common.h"
#include "conv.h"
#include "graphics.h"


void (*drawPixel)(int x, int y, char color) = drawPixelNormal;
void inverseColor(int inverse)
{
	drawPixel = inverse ? drawPixelInverse : drawPixelNormal;
}


uchar mem[DISP_HEIGHT][DISP_MEMWIDTH];
uchar mem2[TITLE_HEIGHT][DISP_MEMWIDTH];
static uchar (*pMem)[DISP_MEMWIDTH] = mem;
int memHeight = DISP_HEIGHT;


void ICACHE_FLASH_ATTR dispSetActiveMemBuf(MemBufType memBuf)
{
	switch (memBuf)
	{
	case MainMemBuf:
		pMem = mem;
		memHeight = DISP_HEIGHT;
		break;
	case SecondaryMemBuf:
		pMem = mem2;
		memHeight = TITLE_HEIGHT;
		break;
	}
}

void ICACHE_FLASH_ATTR dispCopySecMemBufToMain(void)
{
	int i;
	for (i = 0; i < TITLE_HEIGHT; i++)
	{
		os_memcpy(mem[i], mem2[i], DISP_MEMWIDTH);
	}
}

int ICACHE_FLASH_ATTR dispTitleScrollStep(int reset)
{
	int y;
	static int y2 = 0;
	if (reset)
	{
		y2 = 0;
	}

	for (y = 0; y < (TITLE_HEIGHT-1); y++)
	{
		os_memcpy(mem[y], mem[y+1], DISP_MEMWIDTH);
	}
	os_memcpy(mem[y], mem2[y2], DISP_MEMWIDTH);

	if (y2 < (TITLE_HEIGHT-1))
	{
		y2++;
	}
	else
	{
		y2 = 0;
		return TRUE;	// one round done
	}
	return FALSE;
}


void ICACHE_FLASH_ATTR dispFillMem(uchar data, int lines)
{
	uint i;
    for (i = 0; i < memHeight && lines > 0; i++, lines--)
    {
        os_memset(pMem[i], data, DISP_MEMWIDTH);
    }
}

LOCAL void ICACHE_FLASH_ATTR dispDrawBitmap(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize)
{
    int maxBmHeight = memHeight-y;
    if (bmHeight > maxBmHeight)
    {
        bmHeight = maxBmHeight;
    }
    if (bmHeight <= 0)
        return;

    bmWidth /= 8;
    int memX = x/8;
    int bmWidthCpy = bmWidth;
    int maxBmWidth = (DISP_MEMWIDTH-memX);
    if (bmWidthCpy > maxBmWidth)
    {
        bmWidthCpy = maxBmWidth;
    }
    if (bmWidthCpy <= 0)
        return;

    int i;
    if ((memX%4) == 0 && (bmWidthCpy%4) == 0)	// x and width dividable by 4 -> can access flash directly
    {
    	const uchar *pBitmap = (uchar*)bitmap;
		for (i = 0; i < bmHeight; i++, y++)
		{
			os_memcpy(pMem[y]+memX, pBitmap, bmWidthCpy);
			pBitmap += bmWidth;
		}
    }
    else	// x or width not dividable by 4 -> need a temp buffer to avoid alignment issues
    {
    	bitmapSize *= sizeof(uint);		// bitmapSize is dwords
        uchar *temp = (uchar*)os_malloc(bitmapSize);
        uchar *pTemp = temp;
        if (!temp)
        	return;
        os_memcpy(pTemp, bitmap, bitmapSize);
    	for (i = 0; i < bmHeight; i++, y++)
    	{
    		os_memcpy(pMem[y]+memX, pTemp, bmWidthCpy);
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


LOCAL int ICACHE_FLASH_ATTR roundUp(int numToRound, int multiple)
{
    if (multiple == 0)
        return numToRound;

    int remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}

LOCAL char ICACHE_FLASH_ATTR getPixel(int x, int y, int byteWidth, uchar *bitmap)
{
    int xByte = x/8;
    int bitMask = 1<<(7-(x%8));
    int idx = y*byteWidth+xByte;
    return (bitmap[idx] & bitMask) != 0;
}

void ICACHE_FLASH_ATTR drawBitmapPixelByPixel(int x, int y, int bmWidth, int bmHeight, const uint *bitmap, int bitmapSize)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;

	int maxBmHeight = memHeight-y;
    if (bmHeight > maxBmHeight)
    {
        bmHeight = maxBmHeight;
    }
    if (bmHeight <= 0)
        return;

    int byteWidth = roundUp(bmWidth,8)/8;

    int maxBmWidth = DISP_WIDTH-x;
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



void ICACHE_FLASH_ATTR drawPixelNormal(int x, int y, char color)
{
    if (x >= DISP_WIDTH || y >= memHeight)
    {
        return;
    }

    int xByte = x/8;
    int bitMask = 1<<(7-(x%8));
    if (color)
    {
        pMem[y][xByte] |= bitMask;
    }
    else
    {
        pMem[y][xByte] &= ~bitMask;
    }
}

void ICACHE_FLASH_ATTR drawPixelInverse(int x, int y, char color)
{
    if (x >= DISP_WIDTH || y >= memHeight)
    {
        return;
    }

    int xByte = x/8;
    int bitMask = 1<<(7-(x%8));
    if (color)
    {
        pMem[y][xByte] &= ~bitMask;
    }
    else
    {
        pMem[y][xByte] |= bitMask;
    }
}

void ICACHE_FLASH_ATTR drawLine(int x0, int y0, int x1, int y1, char color)
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

void ICACHE_FLASH_ATTR drawRect(int x0, int y0, int x1, int y1, char color)
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
