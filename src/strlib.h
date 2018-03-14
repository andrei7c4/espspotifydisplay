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
    ushort *str;
    int length;
    StrListItem *next;
};
struct StrList
{
    StrListItem *first;
    StrListItem *last;
    int count;
};

void strListAppend(StrList *list, ushort *str, int length);
void strListDraw(const Font *font, int x, int y, StrList *list, const ushort *separator);
int strListEqual(StrList *list1, StrList *list2);
void strListClear(StrList *list);

int drawChar(const Font *font, int x, int y, ushort ch);
int drawStr(const Font *font, int x, int y, const ushort *str, int length);
int drawStr_Latin(const Font *font, int x, int y, const char *str, int length);
int drawStrAlignRight_Latin(const Font *font, int right, int y, const char *str, int length);

int strWidth(const Font *font, const ushort *str);
int strListWidth(const Font *font, StrList *list, const ushort *separator);
int wstrcmp(const ushort *str1, const ushort *str2);



#endif /* STRLIB_H */
