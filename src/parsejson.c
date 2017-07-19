#include <os_type.h>
#include <osapi.h>
#include <mem.h>
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

LOCAL int ICACHE_FLASH_ATTR getNextStringAlloc(struct jsonparse_state *state, int depth, const char *name, char **str)
{
	if (!jumpToNextType(state, depth, JSON_TYPE_PAIR_NAME, name))
		return 0;

	if (jsonparse_next(state) != JSON_TYPE_STRING)
		return 0;

	int bufSize = jsonparse_get_len(state)+1;
	*str = (char*)os_malloc(bufSize);
	return jsonparse_copy_value(state, *str, bufSize);
}

LOCAL int ICACHE_FLASH_ATTR getNextStringAllocUtf8(struct jsonparse_state *state, int depth, const char *name, ushort **str)
{
	char *temp = NULL;
	int length = getNextStringAlloc(state, depth, name, &temp);
	if (!temp) return 0;
	length = decodeUtf8(temp, length, str);
	os_free(temp);
	return length;
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

int ICACHE_FLASH_ATTR parseTrackInfo(const char *json, int jsonLen, TrackInfo *track)
{
	struct jsonparse_state state;
	jsonparse_setup(&state, json, jsonLen);

	if (!jumpToNextType(&state, 1, JSON_TYPE_PAIR_NAME, "item"))
		return ERROR;

	if (!jumpToNextType(&state, 3, JSON_TYPE_ARRAY, NULL))
		return ERROR;

	if ((track->artist.length = getNextStringAllocUtf8(&state, 4, "name", &track->artist.str)) == 0)
		return ERROR;
	// TODO: parse multiple artists


	if ((track->name.length = getNextStringAllocUtf8(&state, 2, "name", &track->name.str)) == 0)
		return ERROR;

	return OK;
}



