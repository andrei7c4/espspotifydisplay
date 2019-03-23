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

typedef enum
{
	trackParseOk = 0,
	trackParseItemErr,
	trackParseAlbumErr1,
	trackParseAlbumErr2,
	trackParseArtistsErr1,
	trackParseArtistsErr2,
	trackParseArtistsErr3,
	trackParseDurationErr,
	trackParseNameErr,
	trackParseIsPlayingErr,
}TrackParseRc;

TrackParseRc parseTrackInfo(const char *json, int jsonLen, TrackInfo *track);

int parseTokens(const char *json, int jsonLen,
		char *accessToken, int accessTokenSize,
		char *refreshToken, int refreshTokenSize,
		int *expiresIn);

#endif /* INCLUDE_PARSEJSON_H_ */
