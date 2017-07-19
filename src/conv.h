#ifndef INCLUDE_CONV_H_
#define INCLUDE_CONV_H_

#include "typedefs.h"

int strtoint(const char *p);
float strtofloat(const char* num);
int decodeUtf8(char *str, int strLen, ushort **utf8str);
int strToWstr(const char *str, int strLen, ushort **wstr);
int u8_toucs(ushort *dest, int sz, char *src, int srcsz);



#endif /* INCLUDE_CONV_H_ */
