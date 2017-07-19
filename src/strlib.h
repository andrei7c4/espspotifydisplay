#ifndef STRLIB_H
#define STRLIB_H

#include "typedefs.h"
#include "fonts.h"


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
    int count;
};

void strSplit(const ushort *str, StrList *list);
void clearStrList(StrList *list);

int drawChar(const Font *font, int x, int y, ushort ch);
int drawStr(const Font *font, int x, int y, const ushort *str, int length);
int drawStr_Latin(const Font *font, int x, int y, const char *str, int length);
int drawStrHighlight_Latin(const Font *font, int x, int y, const char *str);
void drawStrWidthLim(const Font *font, int x, int y, const ushort *str, int width);
int drawStrWordWrapped(int x0, int y0, int x1, int y1, const ushort *str,
		const Font *fontReg, const Font *fontBold, const StrList *boldStrList, int forceDraw);
int replaceLinks(ushort *str, int length);
int replaceHtmlEntities(ushort *str, int length);


#endif /* STRLIB_H */
