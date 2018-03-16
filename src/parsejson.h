#ifndef INCLUDE_PARSEJSON_H_
#define INCLUDE_PARSEJSON_H_

#include "common.h"
#include "strlib.h"

typedef struct
{
	StrBuf name;
	StrList artists;
	StrBuf album;
	int duration;
	int progress;
	int isPlaying;
}TrackInfo;

int parseTokens(const char *json, int jsonLen,
		char *accessToken, int accessTokenSize,
		char *refreshToken, int refreshTokenSize,
		int *expiresIn);

int parseTrackInfo(const char *json, int jsonLen, TrackInfo *track);


#endif /* INCLUDE_PARSEJSON_H_ */
