#include <os_type.h>
#include <osapi.h>
#include <mem.h>
#include "config.h"

int ICACHE_FLASH_ATTR strtoint(const char *p)
{
	int sign = 1;
	if (*p == '-'){
		sign = -1;
		p++;
	}
    int k = 0;
    while (*p) {
        k = (k<<3)+(k<<1)+(*p)-'0';
        p++;
     }
     return k*sign;
}

float ICACHE_FLASH_ATTR strtofloat(const char* s)
{
  float rez = 0, fact = 1;
  int point_seen = 0;
  if (*s == '-'){
	s++;
	fact = -1;
  };
  for (; *s; s++){
	if (*s == '.'){
	  point_seen = 1;
	  continue;
	};
	int d = *s - '0';
	if (d >= 0 && d <= 9){
	  if (point_seen) fact /= 10.0f;
	  rez = rez * 10.0f + (float)d;
	};
  };
  return rez * fact;
}


LOCAL char ICACHE_FLASH_ATTR asciiHexToBin(char asciiHex)
{
	if (asciiHex >= '0' && asciiHex <= '9')
	{
		return asciiHex - '0';
	}
	else if (asciiHex >= 'A' && asciiHex <= 'F')
	{
		return asciiHex - 'A'+10;
	}
	else if (asciiHex >= 'a' && asciiHex <= 'f')
	{
		return asciiHex - 'a'+10;
	}
	return ERROR;
}

LOCAL unsigned int ICACHE_FLASH_ATTR asciiHexStrToBin(char *str, int len)
{
	int i;
	unsigned int value = 0;
	char nibble;
	for (i = 0; i < len; i++)
	{
		nibble = asciiHexToBin(*str++);
		if (nibble == ERROR)
		{
			return ERROR;
		}
		value <<= 4;
		value |= (nibble&0x0F);
	}
	return value;
}

LOCAL int ICACHE_FLASH_ATTR utf8strlen(char *str, int strLen)
{
	int i = 0;
	int length = 0;
	while (*str)
	{
		if (*str == '\\' && *(str+1) == 'u')
		{
			i += 6;
			if (i > strLen)
			{
				return length;
			}
			str += 6;
			length++;
		}
		else if (*str == '\\' && *(str+1) == 'U')
		{
			i += 10;
			if (i > strLen)
			{
				return length;
			}
			str += 10;
		}
		else
		{
			i++;
			str++;
			length++;
		}
	}
	return length;
}

int ICACHE_FLASH_ATTR decodeUtf8(char *str, int strLen, ushort **utf8str)
{
	int i = 0;
	int length = 0;
	*utf8str = (ushort*)os_malloc((utf8strlen(str, strLen) + 1)*sizeof(ushort));
	if (!*utf8str)
	{
		return 0;
	}
	ushort *pUtf8str = *utf8str;
	ushort utf8ch;

	while (*str)
	{
		if (*str == '\\' && *(str+1) == 'u')
		{
			i += 6;
			if (i > strLen)
			{
				*pUtf8str = '\0';
				return length;
			}

			str += 2;
			utf8ch = asciiHexStrToBin(str, 4);
			if (utf8ch != (ushort)ERROR)
			{
				*pUtf8str = utf8ch;
				pUtf8str++;
				length++;
			}
			str += 4;
		}
		else if (*str == '\\' && *(str+1) == 'U')
		{
			i += 10;
			if (i > strLen)
			{
				*pUtf8str = '\0';
				return length;
			}
			str += 10;
		}
		else
		{
			*pUtf8str = *str;
			pUtf8str++;
			length++;
			str++;
			i++;
		}
	}
	*pUtf8str = '\0';
	return length;
}

int ICACHE_FLASH_ATTR strToWstr(const char *str, int strLen, ushort **wstr)
{
	int length = 0;
	*wstr = (ushort*)os_malloc((strLen+1)*sizeof(ushort));
	if (!*wstr)
	{
		return 0;
	}
	ushort *pwstr = *wstr;
	while (*str && strLen > 0)
	{
		*pwstr = *str;
		pwstr++;
		length++;
		str++;
		strLen--;
	}
	return length;
}



LOCAL int ICACHE_FLASH_ATTR trailingBytesForUTF8(unsigned char ch)
{
	if (ch < 0xC0) return 0;
	else if (ch < 0xE0) return 1;
	else if (ch < 0xF0) return 2;
	else if (ch < 0xF8) return 3;
	else if (ch < 0xFC) return 4;
	else return 5;
}

LOCAL const uint offsetsFromUTF8[6] = {
    0x00000000UL, 0x00003080UL, 0x000E2080UL,
    0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

/* 
   Source: http://www.cprogramming.com/tutorial/utf8.c
   
   conversions without error checking
   only works for valid UTF-8, i.e. no 5- or 6-byte sequences
   srcsz = source size in bytes, or -1 if 0-terminated
   sz = dest size in # of wide characters

   returns # characters converted
   dest will always be L'\0'-terminated, even if there isn't enough room
   for all the characters.
   if sz = srcsz+1 (i.e. 2*srcsz+2 bytes), there will always be enough space.
*/
int ICACHE_FLASH_ATTR u8_toucs(ushort *dest, int sz, char *src, int srcsz)
{
    uint ch;
    char *src_end = src + srcsz;
    int nb;
    int i=0;

    while (i < sz-1) {
        nb = trailingBytesForUTF8((unsigned char)*src);
        if (srcsz == -1) {
            if (*src == 0)
                goto done_toucs;
        }
        else {
            if (src + nb >= src_end)
                goto done_toucs;
        }
        ch = 0;
        switch (nb) {
            /* these fall through deliberately */
        case 3: ch += (unsigned char)*src++; ch <<= 6;
        case 2: ch += (unsigned char)*src++; ch <<= 6;
        case 1: ch += (unsigned char)*src++; ch <<= 6;
        case 0: ch += (unsigned char)*src++;
        }
        ch -= offsetsFromUTF8[nb];
		if (ch > 0xFFFF) {
			dest[i++] = ' ';
		}
		else {
			dest[i++] = ch;			
		}
    }
 done_toucs:
    dest[i] = 0;
    return i;
}
