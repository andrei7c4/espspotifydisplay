#include <os_type.h>
#include <osapi.h>
#include <mem.h>
#include "parseobjects.h"
#include "parsejson.h"
#include "common.h"
#include "config.h"
#include "conv.h"

LOCAL void onTrackProgress(const char *value, int length, int type, void *object);
LOCAL void onTrackDuration(const char *value, int length, int type, void *object);
LOCAL void onTrackIsPlaying(const char *value, int length, int type, void *object);
LOCAL void onTrackName(const char *value, int length, int type, void *object);
LOCAL void onArtistName(const char *value, int length, int type, void *object);
LOCAL void onAlbumName(const char *value, int length, int type, void *object);
static void onReleaseDate(const char *value, int length, int type, void *object);

LOCAL void onAccessToken(const char *value, int length, int type, void *object);
LOCAL void onRefreshToken(const char *value, int length, int type, void *object);
LOCAL void onExpiresIn(const char *value, int length, int type, void *object);

LOCAL PathCallback trackCallbacks[] = {
        {.callback = onTrackName},
        {.callback = onArtistName},
        {.callback = onAlbumName},
        {.callback = onReleaseDate},
        {.callback = onTrackDuration},
        {.callback = onTrackProgress},
        {.callback = onTrackIsPlaying}
};

LOCAL PathCallback tokensCallbacks[] = {
        {.callback = onAccessToken},
        {.callback = onRefreshToken},
        {.callback = onExpiresIn}
};


void ICACHE_FLASH_ATTR initPaths(void)
{
    pathInit(&trackCallbacks[0].path, "item", "name");
    pathInit(&trackCallbacks[1].path, "item", "artists", "", "name");
    if (config.showAlbum)
    {
        pathInit(&trackCallbacks[2].path, "item", "album", "name");
        pathInit(&trackCallbacks[3].path, "item", "album", "release_date");
    }
    else
    {
        pathInit(&trackCallbacks[2].path);
        pathInit(&trackCallbacks[3].path);
    }
    pathInit(&trackCallbacks[4].path, "item", "duration_ms");
    pathInit(&trackCallbacks[5].path, "progress_ms");
    pathInit(&trackCallbacks[6].path, "is_playing");

    pathInit(&tokensCallbacks[0].path, "access_token");
    pathInit(&tokensCallbacks[1].path, "refresh_token");
    pathInit(&tokensCallbacks[2].path, "expires_in");
}

TrackParsed ICACHE_FLASH_ATTR parseTrackInfo(const char *json, int jsonLen, TrackInfo *track)
{
    parsejson(json, jsonLen, trackCallbacks, NELEMENTS(trackCallbacks), track);
    if (!config.showAlbum)
    {
        track->parsed |= eTrackAlbumParsed;
    }
    return track->parsed;
}

TokensParsed ICACHE_FLASH_ATTR parseTokens(const char *json, int jsonLen, Tokens *tokens)
{
    parsejson(json, jsonLen, tokensCallbacks, NELEMENTS(tokensCallbacks), tokens);
    return tokens->parsed;
}


LOCAL void ICACHE_FLASH_ATTR onTrackProgress(const char *value, int length, int type, void *object)
{
    //debug("onProgress: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_NUMBER)
    {
        TrackInfo *track = object;
        track->progress = strtoint(value);
        track->parsed |= eTrackProgressParsed;
    }
}

LOCAL void ICACHE_FLASH_ATTR onTrackDuration(const char *value, int length, int type, void *object)
{
    //debug("onTrackDuration: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_NUMBER)
    {
        TrackInfo *track = object;
        track->duration = strtoint(value);
        track->parsed |= eTrackDurationParsed;
    }
}

LOCAL void ICACHE_FLASH_ATTR onTrackIsPlaying(const char *value, int length, int type, void *object)
{
    //debug("onTrackIsPlaying: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_TRUE || type == JSON_TYPE_FALSE)
    {
        TrackInfo *track = object;
        track->isPlaying = (type == JSON_TYPE_TRUE);
        track->parsed |= eTrackIsPlayingParsed;
    }
}

LOCAL void ICACHE_FLASH_ATTR onTrackName(const char *value, int length, int type, void *object)
{
    //debug("onTrackName: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_STRING)
    {
        TrackInfo *track = object;
        if ((track->name.length = decodeUtf8(value, length, &track->name.str)) > 0)
        {
            track->parsed |= eTrackNameParsed;
        }
    }
}

LOCAL void ICACHE_FLASH_ATTR onArtistName(const char *value, int length, int type, void *object)
{
    //debug("onArtistName: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_STRING)
    {
        TrackInfo *track = object;
        ushort *str;
        int strLen;
        if ((strLen = decodeUtf8(value, length, &str)) > 0)
        {
            strListAppend(&track->artists, str, strLen);
            track->parsed |= eTrackArtistsParsed;
        }
    }
}

LOCAL void ICACHE_FLASH_ATTR onAlbumName(const char *value, int length, int type, void *object)
{
    //debug("onAlbumName: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_STRING)
    {
        TrackInfo *track = object;
        ushort *str;
        int strLen;
        if ((strLen = decodeUtf8(value, length, &str)) > 0)
        {
            strListAppend(&track->album, str, strLen);
            track->parsed |= eTrackAlbumParsed;
        }
    }
}

static void onReleaseDate(const char *value, int length, int type, void *object)
{
    //debug("onReleaseDate: value %s, len %d, type %c\n", value, length, (char)type);

    if (type == JSON_TYPE_STRING)
    {
        // cut month and day, leave the year only
        int i;
        for (i = 0; i < length; i++)
        {
            if (value[i] == '-')
            {
                length = i;
            }
        }

        TrackInfo *track = object;
        ushort *str;
        int strLen;
        if ((strLen = decodeUtf8(value, length, &str)) > 0)
        {
            strListAppend(&track->album, str, strLen);
        }
    }
}


LOCAL void ICACHE_FLASH_ATTR onAccessToken(const char *value, int length, int type, void *object)
{
    //debug("onAccessToken: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_STRING)
    {
        Tokens *tokens = object;
        if (length < tokens->accessTokenSize)
        {
            os_memcpy(tokens->accessToken, value, length+1);
            tokens->parsed |= eTokensAccessParsed;
        }
    }
}

LOCAL void ICACHE_FLASH_ATTR onRefreshToken(const char *value, int length, int type, void *object)
{
    //debug("onRefreshToken: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_STRING)
    {
        Tokens *tokens = object;
        if (length < tokens->refreshTokenSize)
        {
            os_memcpy(tokens->refreshToken, value, length+1);
            tokens->parsed |= eTokensRefreshParsed;
        }
    }
}

LOCAL void ICACHE_FLASH_ATTR onExpiresIn(const char *value, int length, int type, void *object)
{
    //debug("onExpiresIn: value %s, len %d, type %c\n", value, length, (char)type);
    if (type == JSON_TYPE_NUMBER)
    {
        Tokens *tokens = object;
        tokens->expiresIn = strtoint(value);
        tokens->parsed |= eTokensExpiresInParsed;
    }
}
