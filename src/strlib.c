#include <ets_sys.h>
#include <string.h>
#include <osapi.h>
#include <mem.h>
#include <limits.h>
#include "graphics.h"
#include "common.h"
#include "debug.h"
#include "strlib.h"


typedef struct WordListItem WordListItem;
typedef struct WordList WordList;
typedef struct LineListItem LineListItem;
typedef struct LineList LineList;

struct WordListItem
{
    const ushort *str;
    int length;
    int width, height, yOffsetMin;
    const Font *font;
    WordListItem *next;
};
struct WordList
{
    WordListItem *first;
    int count;
};

struct LineListItem
{
    WordList words;
    int width, height, yOffsetMin;
    LineListItem *next;
};
struct LineList
{
    LineListItem *first;
    int count;
    int width, height, compressedHeight;
};


LOCAL StrListItem* allocStrListItem(const ushort *str, int length);
LOCAL int strListContains(const StrList *list, const ushort *str, int length, int caseInsensitive);
LOCAL WordListItem* allocWordListItem(const Font *font, const ushort *str, int length);
LOCAL void clearWordList(WordList *wordList);
LOCAL void drawWordList(int x, int y, const WordList *wordList);
LOCAL void strListToWordList(const StrList *strList, WordList *wordList, const Font *fontReg, const Font *fontBold, const StrList *boldStrList);
LOCAL LineListItem* allocLineListItem(void);
LOCAL void clearLineList(LineList *lineList);
LOCAL void drawLineList(int x0, int y0, int maxHeight, const LineList *lineList, int compressed);
LOCAL void splitWordByWidth(WordListItem *word1, int width);
LOCAL void splitWordsToLines(WordList *wordList, LineList *lineList, int lineWidth, int maxLines);
LOCAL void drawStrLenLim(const Font *font, int x, int y, const ushort *str, int length);
LOCAL const uint* getFontBlock(const Font *font, ushort ch);
LOCAL const uint* getCharHeader(const Font *font, ushort *ch);
LOCAL uchar charWidth(const Font *font, ushort ch);
LOCAL uchar charHeight(const Font *font, ushort ch);
LOCAL int charYoffset(const Font *font, ushort ch);
LOCAL int strLength(const ushort *str);
LOCAL int strWidth(const Font *font, const ushort *str);
LOCAL int strWidthLenLim(const Font *font, const ushort *str, int length);
LOCAL int strHeightLenLim(const Font *font, const ushort *str, int length);
LOCAL int strMinYoffset(const Font *font, const ushort *str, int length);
LOCAL int strLength_widthLim(const Font *font, const ushort *str, int width, int *strWidth);
LOCAL int isAlphanumeric(ushort ch);
LOCAL int isDelimeter(ushort ch);
LOCAL int strNextWordLength(const ushort *str);
LOCAL ushort chToLower(ushort ch);
LOCAL int strStartsWith(const ushort *str1, int len1, const ushort *str2, int len2, int caseInsensitive);
LOCAL int strIsEqual(const ushort *str1, const ushort *str2, int length);
LOCAL const ushort* wstrnstr(const ushort *haystack, int haystackLen, const ushort *needle, int needleLen);


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

void ICACHE_FLASH_ATTR strSplit(const ushort *str, StrList *list)
{
	list->first = NULL;
    list->count = 0;
    if (!str || !*str)
    {
        return;
    }
    StrListItem *item = allocStrListItem(str, strNextWordLength(str));
    if (!item)
    {
        return;
    }
    list->first = item;
    list->count = 1;
    str += item->length;
    while (*str)
    {
        item->next = allocStrListItem(str, strNextWordLength(str));
        item = item->next;
        if (!item)
        {
            break;
        }
        list->count++;
        str += item->length;
    }
}

void ICACHE_FLASH_ATTR clearStrList(StrList *list)
{
    StrListItem *item = list->first;
    StrListItem *next;
    while (item)
    {
        next = item->next;
        os_free(item);
        item = next;
    }
    list->first = NULL;
    list->count = 0;
}

LOCAL int ICACHE_FLASH_ATTR strListContains(const StrList *list, const ushort *str, int length, int caseInsensitive)
{
	if (!list || !str)
	{
		return FALSE;
	}
    int i = 0;
	StrListItem *item = list->first;
    while (i < list->count && item)
    {
    	if (strStartsWith(str, length, item->str, item->length, caseInsensitive))
    	{
    		return TRUE;
    	}
        item = item->next;
        i++;
    }
    return FALSE;
}



LOCAL WordListItem* ICACHE_FLASH_ATTR allocWordListItem(const Font *font, const ushort *str, int length)
{
    WordListItem *item = (WordListItem*)os_malloc(sizeof(WordListItem));
    if (!item)
    {
    	return NULL;
    }
    item->str = str;
    item->length = length;
    item->width = strWidthLenLim(font, str, length);
    item->height = strHeightLenLim(font, str, length);
    item->yOffsetMin = strMinYoffset(font, str, length);
    item->font = font;
    item->next = NULL;
    return item;
}

LOCAL void ICACHE_FLASH_ATTR clearWordList(WordList *wordList)
{
    WordListItem *item = wordList->first;
    WordListItem *next;
    while (item)
    {
        next = item->next;
        os_free(item);
        item = next;
    }
    wordList->first = NULL;
    wordList->count = 0;
}

LOCAL void ICACHE_FLASH_ATTR drawWordList(int x, int y, const WordList *wordList)
{
    WordListItem *word = wordList->first;
    int count = wordList->count;
    while (word && count > 0)
    {
        drawStrLenLim(word->font, x, y, word->str, word->length);
        x += word->width;
        word = word->next;
        count--;
    }
}

LOCAL void ICACHE_FLASH_ATTR strListToWordList(const StrList *strList, WordList *wordList, const Font *fontReg, const Font *fontBold, const StrList *boldStrList)
{
    if (!strList->first || strList->count == 0)
    {
        wordList->first = NULL;
        wordList->count = 0;
        return;
    }
    StrListItem *str = strList->first;

    const Font *font = (fontBold && strListContains(boldStrList, str->str, str->length, TRUE)) ? fontBold : fontReg;
    WordListItem *item = allocWordListItem(font, str->str, str->length);
    if (!item)
    {
        return;
    }
    wordList->first = item;
    wordList->count = 1;
    str = str->next;
    while (str)
    {
    	font = (fontBold && strListContains(boldStrList, str->str, str->length, TRUE)) ? fontBold : fontReg;
        item->next = allocWordListItem(font, str->str, str->length);
        item = item->next;
        if (!item)
        {
            break;
        }
        wordList->count++;
        str = str->next;
    }
}



LOCAL LineListItem* ICACHE_FLASH_ATTR allocLineListItem(void)
{
    LineListItem *line = (LineListItem*)os_malloc(sizeof(LineListItem));
    if (!line)
    {
        return NULL;
    }
    line->width = 0;
    line->height = 0;
    line->yOffsetMin = INT_MAX;
    line->next = NULL;
    line->words.first = NULL;
    line->words.count = 0;
    return line;
}

LOCAL void ICACHE_FLASH_ATTR clearLineList(LineList *lineList)
{
    LineListItem *line = lineList->first;
    LineListItem *next;
    while (line)
    {
        next = line->next;
        os_free(line);
        line = next;
    }
}

LOCAL void ICACHE_FLASH_ATTR drawLineList(int x0, int y0, int maxHeight, const LineList *lineList, int compressed)
{
    LineListItem *line = lineList->first;
    int y = y0;
    int height = 0;
    int yOffset;
    int lineHeight;
    while (line)
    {
        yOffset = compressed ? line->yOffsetMin : 0;
        lineHeight = line->height - yOffset;
        if ((height + lineHeight) > maxHeight)
        {
            return;
        }

        drawWordList(x0, y - yOffset, &line->words);
        height += lineHeight;
        y += lineHeight;

        line = line->next;
    }
}



LOCAL void ICACHE_FLASH_ATTR splitWordByWidth(WordListItem *word1, int width)
{
    int origLen = word1->length;
    word1->length = strLength_widthLim(word1->font, word1->str, width, &word1->width);
    word1->height = strHeightLenLim(word1->font, word1->str, word1->length);
    word1->yOffsetMin = strMinYoffset(word1->font, word1->str, word1->length);

    WordListItem *word2 = allocWordListItem(word1->font, word1->str + word1->length, origLen - word1->length);
    word2->next = word1->next;
    word1->next = word2;
}

LOCAL void ICACHE_FLASH_ATTR splitWordsToLines(WordList *wordList, LineList *lineList, int lineWidth, int maxLines)
{
    if (wordList->count == 0 || maxLines < 1)
    {
        os_memset(lineList, 0, sizeof(LineList));
        return;
    }

    LineListItem *line = allocLineListItem();
    if (!line)
    {
    	return;
    }
    lineList->first = line;
    lineList->count = 1;
    lineList->width = lineWidth;
    WordListItem *word = wordList->first;
    while (word)
    {
        if (!line->words.first)
        {
            line->words.first = word;
        }

        if ((line->width + word->width) > lineWidth)    // will not fit on this line
        {
            if (word->width > lineWidth)    // will not fit on single line
            {
                // can we fit at least one char on this line?
                if (line->width + charWidth(word->font, *word->str) <= lineWidth)
                {
                    splitWordByWidth(word, lineWidth - line->width);
                    wordList->count++;
                }
                else    // this line is full
                {
                    if (lineList->count < maxLines)
                    {
                        line->next = allocLineListItem();
                        if (!line->next)
                        {
                        	break;
                        }
                        line = line->next;
                        lineList->count++;
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else    // will fit on next line
            {
                if (lineList->count < maxLines)
                {
                    line->next = allocLineListItem();
                    if (!line->next)
                    {
                    	break;
                    }
                    line = line->next;
                    lineList->count++;
                    continue;
                }
                else
                {
                    break;
                }
            }
        }

        line->words.count++;
        line->width += word->width;
        if (word->height > line->height)
        {
            line->height = word->height;
        }
        if (word->yOffsetMin < line->yOffsetMin)
        {
            line->yOffsetMin = word->yOffsetMin;
        }
        word = word->next;
    }

    int pixelsBetweenLines = 1;
    line = lineList->first;
    while (line)
    {
        if (line->next)
        {
            line->height += pixelsBetweenLines;
        }
        else
        {
            break;
        }
        line = line->next;
    }

    // count overall height
    lineList->height = 0;
    lineList->compressedHeight = 0;
    line = lineList->first;
    while (line)
    {
        lineList->height += line->height;
        lineList->compressedHeight +=
                line->yOffsetMin == INT_MAX ? line->height :
                (line->height - line->yOffsetMin);
        line = line->next;
    }
}










int ICACHE_FLASH_ATTR drawStrWordWrapped(int x0, int y0, int x1, int y1, const ushort *str,
		const Font *fontReg, const Font *fontBold, const StrList *boldStrList, int forceDraw)
{
    x0 = clampInt(x0, 0, DISP_WIDTH-1);
    x1 = clampInt(x1, 0, DISP_WIDTH-1);
    y0 = clampInt(y0, 0, memHeight-1);
    y1 = clampInt(y1, 0, memHeight-1);

    int width = x1-x0+1;
    int height = y1-y0+1;

    StrList list;
    strSplit(str, &list);

    WordList words;
    strListToWordList(&list, &words, fontReg, fontBold, boldStrList);
    //printWordList(&words);

    LineList lines;
    splitWordsToLines(&words, &lines, width, 20);
    //printLineList(&lines);

//	int fit = lines.compressedHeight <= height;
//	if (fit || forceDraw)
//	{
//		drawLineList(x0, y0, height, &lines, lines.height > height);
//	}
    int fit;
    if (forceDraw)
    {
    	fit = lines.compressedHeight <= height;
    	drawLineList(x0, y0, height, &lines, lines.height > height);
    }
    else
    {
    	fit = lines.height <= height;
    	if (fit)
    	{
        	drawLineList(x0, y0, height, &lines, FALSE);
    	}
    }

	clearLineList(&lines);
    clearWordList(&words);
    clearStrList(&list);
	return fit;
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

    int minYoffset = strMinYoffset(font, str, length);
    y -= minYoffset;

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

int ICACHE_FLASH_ATTR drawStrHighlight_Latin(const Font *font, int x, int y, const char *str)
{
	ushort *strbuf = NULL;
	int width = 0;
	int height, minYoffset;
	int strLen = strToWstr(str, os_strlen(str), &strbuf);
	if (strLen && strbuf)
	{
		width = strWidthLenLim(font, strbuf, strLen);
		height = strHeightLenLim(font, strbuf, strLen);
		minYoffset = strMinYoffset(font, strbuf, strLen);

		int x1 = x+width+1;
		int y1 = y+height-minYoffset+1;
		drawRect(x, y, x1, y1, 1);
		drawPixel(x, y, 0);
		drawPixel(x1, y, 0);
		drawPixel(x, y1, 0);
		drawPixel(x1, y1, 0);

		inverseColor(TRUE);
		drawStr(font, x+1, y+1, strbuf, strLen);
		inverseColor(FALSE);
	}
	os_free(strbuf);
	return width+2;
}

void ICACHE_FLASH_ATTR drawStrWidthLim(const Font *font, int x, int y, const ushort *str, int width)
{
    while (*str)
    {
        ushort ch = *str;
        int chWidth = charWidth(font, ch);
        if (width < chWidth)
        {
            return;
        }
        drawChar(font, x, y, ch);
        x += chWidth;
        width -= chWidth;
        str++;
    }
}

LOCAL void ICACHE_FLASH_ATTR drawStrLenLim(const Font *font, int x, int y, const ushort *str, int length)
{
    while (*str && length > 0)
    {
        ushort ch = *str;
        int chWidth = drawChar(font, x, y, ch);
        x += chWidth;
        str++;
        length--;
    }
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
	int strWidth = 0;
    while (*str)
    {
        ushort ch = *str;
        int chWidth = charWidth(font, ch);
        strWidth += chWidth;
        str++;
    }
    return strWidth;
}

LOCAL int ICACHE_FLASH_ATTR strWidthLenLim(const Font *font, const ushort *str, int length)
{
    int strWidth = 0;
    while (*str && length > 0)
    {
        ushort ch = *str;
        int chWidth = charWidth(font, ch);
        strWidth += chWidth;
        str++;
        length--;
    }
    return strWidth;
}

LOCAL int ICACHE_FLASH_ATTR strHeightLenLim(const Font *font, const ushort *str, int length)
{
    int maxHeight = 0;
    int height, yOffset;
    ushort ch;
    while (*str && length > 0)
    {
        ch = *str;
        height = charHeight(font, ch);
        yOffset = charYoffset(font, ch);
		if (yOffset >= 0)
		{
            height += yOffset;			
		}
        if (height > maxHeight)
        {
            maxHeight = height;
        }
        str++;
        length--;
    }
    return maxHeight;
}

LOCAL int ICACHE_FLASH_ATTR strMinYoffset(const Font *font, const ushort *str, int length)
{
    int minYoffset = INT_MAX;
    int yOffset;
    ushort ch;
    while (*str && length > 0)
    {
        ch = *str;
		yOffset = charYoffset(font, ch);
		if (yOffset >= 0 && yOffset < minYoffset)
		{
			minYoffset = yOffset;
		}
        str++;
        length--;
    }
    return minYoffset;
}

LOCAL int ICACHE_FLASH_ATTR strLength_widthLim(const Font *font, const ushort *str, int width, int *strWidth)
{
    int length = 0;
    int chWidth;
    *strWidth = 0;
    while (*str)
    {
        chWidth = charWidth(font, *str);
        if ((*strWidth+chWidth) > width)
        {
            return length;
        }
        *strWidth += chWidth;
        length++;
        str++;
    }
    return length;
}



LOCAL int isAlphanumeric(ushort ch)
{
    if ((ch >= '0' && ch <= '9') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z'))
    {
        return TRUE;
    }
    return FALSE;
}

LOCAL int isDelimeter(ushort ch)
{
	return (ch <= 127 && !isAlphanumeric(ch) && ch != '#' && ch != '@');
}

LOCAL int ICACHE_FLASH_ATTR strNextWordLength(const ushort *str)
{
    int length = 0;
    while (*str)
    {
		length++;
        ushort ch = *str;
        if (isDelimeter(ch))
        {
            break;
        }
        else
        {
            str++;
        }
    }
    return length;
}



/**
 * Only works for Latin+Latin supp., Latin ExtA and Cyrillic.
 * Yes, I know it's far from perfect, but it's good enough for
 * this application, since it is only used while comparing
 * strings for highlighting keywords.
 * And I am not porting ICU libraries to ESP8266.
 */
LOCAL ushort ICACHE_FLASH_ATTR chToLower(ushort ch)
{
	if ((ch >= 'A' && ch <= 'Z') ||
	    (ch >= 0xC0 && ch <= 0xDE) ||
		(ch >= 0x410 && ch <= 0x42F))
	{
		return ch + 32;
	}
	else if ((ch >= 0x100 && ch <= 0x17F && !(ch & 1)) ||
			 (ch >= 0x460 && ch <= 0x4FF && !(ch & 1)))
	{
		return ch + 1;
	}
	else if (ch >= 0x400 && ch <= 0x40F)
	{
		return ch + 80;
	}
	return ch;
}

/// Case insensitive only works for Latin, Latin supp., Latin ExtA and Cyrillic.
LOCAL int ICACHE_FLASH_ATTR strStartsWith(const ushort *str1, int len1, const ushort *str2, int len2, int caseInsensitive)
{
	if (len1 < 1 || len2 < 1)
	{
		return (len1 == len2);
	}
	// exclude delimeters
	if (isDelimeter(str1[len1-1])) len1--;
	if (isDelimeter(str2[len2-1])) len2--;
	// exclude hashtags
	if (*str1 == '#')
	{
		str1++;
		len1--;
	}
	if (*str2 == '#')
	{
		str2++;
		len2--;
	}
    
	while (len2 > 0)
	{
		if (!*str1 || !*str2)
		{
			return FALSE;
		}
		if ((caseInsensitive && (chToLower(*str1) != chToLower(*str2))) ||
		   (!caseInsensitive && (*str1 != *str2)))
		{
			return FALSE;
		}
		str1++;
		str2++;
		len2--;
	}
	return TRUE;
}



LOCAL int ICACHE_FLASH_ATTR strIsEqual(const ushort *str1, const ushort *str2, int length)
{
	if (length < 1)
	{
		return FALSE;
	}
	while (length > 0)
	{
		if (!*str1 || !*str2 || (*str1 != *str2))
		{
			return FALSE;
		}
		str1++;
		str2++;
		length--;
	}
	return TRUE;
}

LOCAL const ushort* ICACHE_FLASH_ATTR wstrnstr(const ushort *haystack, int haystackLen, const ushort *needle, int needleLen)
{
	haystackLen -= needleLen;
	while (haystackLen >= 0)
	{
		if (strIsEqual(haystack, needle, needleLen))
		{
			return haystack;					
		}
		haystack++;
		haystackLen--;
	}
	return NULL;
}

LOCAL int ICACHE_FLASH_ATTR replaceStr(ushort *haystack, int haystackLen, const ushort *needle, int needleLen, const ushort *replace, int replaceLen, ushort separator)
{
	if (replaceLen > needleLen)
	{
		return 0;
	}
	const ushort *src = haystack;
	ushort *dst = haystack;
	int replaced = 0;
	while (haystackLen > 0)
	{
		const ushort *pos = wstrnstr(src, haystackLen, needle, needleLen);
		if (pos)	// needle found
		{
			// copy chars before the needle
			int words = (pos-src);
			os_memcpy(dst, src, words*2);
			dst += words;
			src += words;
			haystackLen -= words;

			// copy replacement
			os_memcpy(dst, replace, replaceLen*2);
			dst += replaceLen;

			if (separator)
			{
				// skip until the separator
				while (haystackLen > 0 && *src != separator)
				{
					src++;
					haystackLen--;
				}
			}
			else
			{
				// skip the needle chars
				src += needleLen;
				haystackLen -= needleLen;
			}

			replaced++;
		}
		else	// no more strings found, copy rest
		{
			os_memcpy(dst, src, haystackLen*2);
			dst += haystackLen;
			break;
		}
	}
	*dst = '\0';
	return replaced;
}

int ICACHE_FLASH_ATTR replaceLinks(ushort *str, int length)
{
	const ushort http[] = {'h','t','t','p'};
	const ushort httpCap[] = {'H','T','T','P'};
	const ushort linkPic = LINK_PICTOGRAM_CODE;
	int replaced = 0;
	replaced += replaceStr(str, length, http, NELEMENTS(http), &linkPic, 1, ' ');
	replaced += replaceStr(str, length, httpCap, NELEMENTS(httpCap), &linkPic, 1, ' ');
	return replaced;
}

int replaceHtmlEntities(ushort *str, int length)
{
	const ushort amp[] = {'&','a','m','p',';'};
	const ushort lt[] = {'&','l','t',';'};
	const ushort gt[] = {'&','g','t',';'};
	const ushort ampChar = '&';
	const ushort ltChar = '<';
	const ushort gtChar = '>';
	int replaced = 0;
	replaced += replaceStr(str, length, amp, NELEMENTS(amp), &ampChar, 1, 0);
	replaced += replaceStr(str, length, lt, NELEMENTS(lt), &ltChar, 1, 0);
	replaced += replaceStr(str, length, gt, NELEMENTS(gt), &gtChar, 1, 0);
	return replaced;
}

