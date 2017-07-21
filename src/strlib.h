#ifndef STRLIB_H
#define STRLIB_H

#include "typedefs.h"
#include "fonts.h"


typedef struct
{
	ushort *str;
	int length;
}StrBuf;

typedef struct StrListItem StrListItem;
typedef struct StrList StrList;

struct StrListItem
{
    const ushort *str;
    int length;
    StrListItem *next;
};
struct StrList
{
    StrListItem *first;
    StrListItem *last;
    int count;
};

void strSplit(const ushort *str, StrList *list);
void strListAppend(StrList *list, const ushort *str, int length);
int strListEqual(StrList *list1, StrList *list2);
void strListDraw(const Font *font, int x, int y, StrList *list, StrBuf *separator);
void strListClear(StrList *list);

int drawChar(const Font *font, int x, int y, ushort ch);
int drawStr(const Font *font, int x, int y, const ushort *str, int length);
int drawStr_Latin(const Font *font, int x, int y, const char *str, int length);
int drawStrAlignRight_Latin(const Font *font, int right, int y, const char *str, int length);
int drawStrHighlight_Latin(const Font *font, int x, int y, const char *str);
void drawStrWidthLim(const Font *font, int x, int y, const ushort *str, int width);

int wstrcmp(const ushort *str1, const ushort *str2);
int replaceLinks(ushort *str, int length);
int replaceHtmlEntities(ushort *str, int length);


#endif /* STRLIB_H */
