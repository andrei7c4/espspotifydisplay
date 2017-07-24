#include <ets_sys.h>
#include <string.h>
#include <osapi.h>
#include <mem.h>
#include <limits.h>
#include "graphics.h"
#include "common.h"
#include "debug.h"
#include "strlib.h"



LOCAL const uint* getFontBlock(const Font *font, ushort ch);
LOCAL const uint* getCharHeader(const Font *font, ushort *ch);
LOCAL uchar charWidth(const Font *font, ushort ch);
LOCAL uchar charHeight(const Font *font, ushort ch);
LOCAL int charYoffset(const Font *font, ushort ch);
LOCAL int strLength(const ushort *str);
LOCAL int strWidth(const Font *font, const ushort *str);


LOCAL StrListItem* ICACHE_FLASH_ATTR allocStrListItem(const ushort *str, int length)
{
    StrListItem *item = (StrListItem*)os_malloc(sizeof(StrListItem));
    if (item)
    {
        item->str = str;
        item->length = length;
        item->next = NULL;
    }
    return item;
}

void ICACHE_FLASH_ATTR strListAppend(StrList *list, const ushort *str, int length)
{
	StrListItem *item = allocStrListItem(str, length);
    if (!item)
    {
        return;
    }
	if (!list->first)
	{
		list->first = item;
		list->last = item;
		list->count = 1;
	}
	else
	{
		list->last->next = item;
		list->last = item;
		list->count++;
	}
}

void ICACHE_FLASH_ATTR strListDraw(const Font *font, int x, int y, StrList *list, StrBuf *separator)
{
    StrListItem *item = list->first;
    while (item)
    {
    	x += drawStr(font, x, y, item->str, item->length);
    	item = item->next;
    	if (item)
    	{
        	x += drawStr(font, x, y, separator->str, separator->length);
    	}
    }
}

int ICACHE_FLASH_ATTR strListEqual(StrList *list1, StrList *list2)
{
	if (list1->count != list2->count)
	{
		return FALSE;
	}
	StrListItem *item1 = list1->first;
	StrListItem *item2 = list2->first;
	while (item1 && item2)
	{
		if (wstrcmp(item1->str, item2->str))
		{
			return FALSE;
		}
		item1 = item1->next;
		item2 = item2->next;
	}
	return TRUE;
}

void ICACHE_FLASH_ATTR strListClear(StrList *list)
{
    StrListItem *item = list->first;
    StrListItem *next;
    while (item)
    {
    	os_free(item->str);
        next = item->next;
        os_free(item);
        item = next;
    }
    list->first = NULL;
    list->last = NULL;
    list->count = 0;
}




int ICACHE_FLASH_ATTR drawChar(const Font *font, int x, int y, ushort ch)
{
	const uint *block = getFontBlock(font, ch);
    if (!block)		// block not found
    {
		ch = REPLACEMENT_CHAR;
		block = getFontBlock(font, ch);
		if (!block)
			return 0;
    }
	int first = spiFlashReadDword(block); //block[0];
	int chIdx = ch-(first-2);
	int chOffset = spiFlashReadDword(block+chIdx); //*(block+chIdx);
	if (!chOffset)	// char not found
	{
		ch = REPLACEMENT_CHAR;
		block = getFontBlock(font, ch);
		if (!block)
			return 0;
		first = spiFlashReadDword(block); //block[0];
		chIdx = ch-(first-2);
		chOffset = spiFlashReadDword(block+chIdx); //*(block+chIdx);
		if (!chOffset)
            return 0;
	}
    //volatile const uint *pHeader = block+chOffset;
    const uint *pHeader = block+chOffset;
	uint header = spiFlashReadDword(pHeader); //*pHeader;
    uchar chWidth = header>>24;
    if (ch == ' ')  // skip space
    {
        return chWidth;
    }
    uchar chHeight = header>>16;
    uchar bitmapSize = header>>8;
    uchar yoffset = header;
	//debug("0x%08X, %u, %u, %u, %u\n", header, chWidth, chHeight, bitmapSize, yoffset);

	drawBitmapPixelByPixel(x, y+yoffset, chWidth, chHeight, (pHeader+1), bitmapSize);
    return chWidth;
}

int ICACHE_FLASH_ATTR drawStr(const Font *font, int x, int y, const ushort *str, int length)
{
	if (length < 0)
	{
		length = strLength(str);
	}

    //int minYoffset = strMinYoffset(font, str, length);
    //y -= minYoffset;

	int strWidth = 0;
    while (*str && length > 0)
    {
        ushort ch = *str;
        int chWidth = drawChar(font, x, y, ch);
        x += chWidth;
        strWidth += chWidth;
        str++;
        length--;
    }
    return strWidth;
}

int ICACHE_FLASH_ATTR drawStr_Latin(const Font *font, int x, int y, const char *str, int length)
{
	ushort *strbuf = NULL;
	int width = 0;	
	if (length < 0)
	{
		length = os_strlen(str);
	}
	int strLen = strToWstr(str, length, &strbuf);
	if (strLen && strbuf)
	{
		width = drawStr(font, x, y, strbuf, strLen);
	}
	os_free(strbuf);
	return width;
}

int ICACHE_FLASH_ATTR drawStrAlignRight_Latin(const Font *font, int right, int y, const char *str, int length)
{
	ushort *strbuf = NULL;
	int width = 0;
	if (length < 0)
	{
		length = os_strlen(str);
	}
	int strLen = strToWstr(str, length, &strbuf);
	if (strLen && strbuf)
	{
		width = strWidth(font, strbuf);
		width = drawStr(font, right-width, y, strbuf, strLen);
	}
	os_free(strbuf);
	return width;
}





LOCAL const uint* ICACHE_FLASH_ATTR getFontBlock(const Font *font, ushort ch)
{
	int i;
	const uint *block;
	int first, last;
	for (i = 0; i < font->count; i++)
	{
		block = font->blocks[i];
		first = spiFlashReadDword(block); //block[0];
		last = spiFlashReadDword(block+1); //block[1];
		if (ch >= first && ch <= last)
		{
			return block;
		}
	}
	return NULL;
}

LOCAL const uint* ICACHE_FLASH_ATTR getCharHeader(const Font *font, ushort *ch)
{
	const uint *block = getFontBlock(font, *ch);
    if (!block)		// block not found
    {
		*ch = REPLACEMENT_CHAR;
		block = getFontBlock(font, *ch);
		if (!block)
			return NULL;
    }
	int first = spiFlashReadDword(block);
	int chIdx = (*ch)-(first-2);
	int chOffset = spiFlashReadDword(block+chIdx);
	if (!chOffset)	// char not found
	{
		*ch = REPLACEMENT_CHAR;
		block = getFontBlock(font, *ch);
		if (!block)
			return NULL;
		first = spiFlashReadDword(block);
		chIdx = (*ch)-(first-2);
		chOffset = spiFlashReadDword(block+chIdx);
		if (!chOffset)
            return NULL;
	}
    return (block+chOffset);
}

LOCAL uchar ICACHE_FLASH_ATTR charWidth(const Font *font, ushort ch)
{
    const uint *pHeader = getCharHeader(font, &ch);
	if (!pHeader)
	{
		return 0;
	}
	uint header = spiFlashReadDword(pHeader); //*pHeader;
    return header>>24;
}

LOCAL uchar ICACHE_FLASH_ATTR charHeight(const Font *font, ushort ch)
{
    const uint *pHeader = getCharHeader(font, &ch);
	if (!pHeader)
	{
		return 0;
	}
	uint header = spiFlashReadDword(pHeader);
    return header>>16;
}

LOCAL int ICACHE_FLASH_ATTR charYoffset(const Font *font, ushort ch)
{
	if (ch == ' ')
	{
		return -1;
	}
    const uint *pHeader = getCharHeader(font, &ch);
	if (!pHeader || ch == ' ')	// ch got replaced with space
	{
		return -1;
	}
	uint header = spiFlashReadDword(pHeader);
    return header&0xFF;
}




LOCAL int strLength(const ushort *str)
{
	const ushort *pStr = str;
	while (*pStr)
	{
		pStr++;
	}
	return pStr - str;
}

LOCAL int ICACHE_FLASH_ATTR strWidth(const Font *font, const ushort *str)
{
	int width = 0;
    while (*str)
    {
        ushort ch = *str;
        width += charWidth(font, ch);
        str++;
    }
    return width;
}

int ICACHE_FLASH_ATTR wstrcmp(const ushort *str1, const ushort *str2)
{
    while (*str1 && (*str1 == *str2))
    {
    	str1++;
    	str2++;
    }
    return *str1 - *str2;
}
