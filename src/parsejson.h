#ifndef INCLUDE_PARSEJSON_H_
#define INCLUDE_PARSEJSON_H_

#include "common.h"

typedef struct
{
	char *buf;
	int bufSize;
	int strLen;
}StrBuf;

int parseTokens(const char *json, int jsonLen,
		char *accessToken, int accessTokenSize,
		char *refreshToken, int refreshTokenSize,
		int *expiresIn);

int parseTrackInfo(const char *json, int jsonLen,
		StrBuf *idStr, StrBuf *trackName, StrBuf *artist);


#endif /* INCLUDE_PARSEJSON_H_ */
