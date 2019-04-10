#ifndef INCLUDE_PARSEOBJECTS_H_
#define INCLUDE_PARSEOBJECTS_H_

#include "strlib.h"


typedef enum
{
    eTrackNameParsed      = 0x01,
    eTrackArtistsParsed   = 0x02,
    eTrackAlbumParsed     = 0x04,
    eTrackDurationParsed  = 0x08,
    eTrackProgressParsed  = 0x10,
    eTrackIsPlayingParsed = 0x20,
    eTrackAllParsed = eTrackNameParsed     |
                      eTrackArtistsParsed  |
                      eTrackAlbumParsed    |
                      eTrackDurationParsed |
                      eTrackProgressParsed |
                      eTrackIsPlayingParsed
}TrackParsed;

typedef struct
{
    StrBuf name;
    StrList artists;
    StrList album;
    int duration;
    int progress;
    int isPlaying;
    TrackParsed parsed;
}TrackInfo;


typedef enum
{
    eTokensAccessParsed    = 0x01,
    eTokensRefreshParsed   = 0x02,
    eTokensExpiresInParsed = 0x04,
    eTokensAllParsed = eTokensAccessParsed  |
                       eTokensRefreshParsed |
                       eTokensExpiresInParsed
}TokensParsed;

typedef struct
{
    char *accessToken;
    int accessTokenSize;
    char *refreshToken;
    int refreshTokenSize;
    int expiresIn;
    TokensParsed parsed;
}Tokens;


void initPaths(void);

TrackParsed parseTrackInfo(const char *json, int jsonLen, TrackInfo *track);

TokensParsed parseTokens(const char *json, int jsonLen, Tokens *tokens);


#endif /* INCLUDE_PARSEOBJECTS_H_ */
