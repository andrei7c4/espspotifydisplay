#include <os_type.h>
#include <osapi.h>
#include "contikijson/jsonparse.h"
#include "contikijson/jsontree.h"
#include "parsejson.h"
#include "common.h"
#include "config.h"
#include "conv.h"


LOCAL int ICACHE_FLASH_ATTR jumpToNextType(struct jsonparse_state *state, int depth, int type, const char *name)
{
	char buf[50];
	int json_type;
	while ((json_type = jsonparse_next(state)) != 0)
	{
		if (depth == state->depth && json_type == type)
		{
			if (name)
			{
				jsonparse_copy_value(state, buf, sizeof(buf));
				if (!os_strncmp(buf, name, sizeof(buf)))
				{
					return TRUE;
				}
			}
			else
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

LOCAL int ICACHE_FLASH_ATTR getNextString(struct jsonparse_state *state, int depth, const char *name, char *str, int strSize)
{
	if (!jumpToNextType(state, depth, JSON_TYPE_PAIR_NAME, name))
		return 0;

	if (jsonparse_next(state) != JSON_TYPE_STRING)
		return 0;

	return jsonparse_copy_value(state, str, strSize);
}

LOCAL int ICACHE_FLASH_ATTR getNextInt(struct jsonparse_state *state, int depth, const char *name, int *value)
{
	char buf[11];
	if (!jumpToNextType(state, depth, JSON_TYPE_PAIR_NAME, name))
		return FALSE;

	if (jsonparse_next(state) != JSON_TYPE_NUMBER)
		return FALSE;

	jsonparse_copy_value(state, buf, sizeof(buf));
	*value = strtoint(buf);
	return TRUE;
}


int ICACHE_FLASH_ATTR parseTokens(const char *json, int jsonLen,
		char *accessToken, int accessTokenSize,
		char *refreshToken, int refreshTokenSize,
		int *expiresIn)
{
	struct jsonparse_state state;
	jsonparse_setup(&state, json, jsonLen);

	if (!getNextString(&state, 1, "access_token", accessToken, accessTokenSize))
		return ERROR;

	if (!getNextInt(&state, 1, "expires_in", expiresIn))
		return ERROR;

	getNextString(&state, 1, "refresh_token", refreshToken, refreshTokenSize);

	return OK;
}

int ICACHE_FLASH_ATTR parseTrackInfo(const char *json, int jsonLen,
		StrBuf *idStr, StrBuf *trackName, StrBuf *artist)
{
	struct jsonparse_state state;
	jsonparse_setup(&state, json, jsonLen);

	if (!jumpToNextType(&state, 1, JSON_TYPE_PAIR_NAME, "item"))
		return ERROR;

	if (!jumpToNextType(&state, 3, JSON_TYPE_ARRAY, NULL))
		return ERROR;

	if ((artist->strLen = getNextString(&state, 4, "name", artist->buf, artist->bufSize)) == 0)
		return ERROR;
	// TODO: parse multiple artists

	if ((idStr->strLen = getNextString(&state, 2, "id", idStr->buf, idStr->bufSize)) == 0)
		return ERROR;

	if ((trackName->strLen = getNextString(&state, 2, "name", trackName->buf, trackName->bufSize)) == 0)
		return ERROR;

	return OK;
}



