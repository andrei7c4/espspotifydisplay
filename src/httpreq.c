#include <os_type.h>
#include <osapi.h>
#include <ip_addr.h>
#include <lwip/err.h>
#include <lwip/dns.h>
#include <user_interface.h>
#include <espconn.h>
#include <mem.h>
#include <sntp.h>
#include "typedefs.h"
#include "config.h"
#include "debug.h"
#include "httpreq.h"


LOCAL const char *spotifyTokenEndpoint = "/api/token";
LOCAL const char *spotifyCurrentlyPlayingEndpoint = "/v1/me/player/currently-playing";
LOCAL const char *spotifyPlayerPlayEndpoint = "/v1/me/player/play";
LOCAL const char *spotifyPlayerPauseEndpoint = "/v1/me/player/pause";
LOCAL const char *spotifyPlayerNextEndpoint = "/v1/me/player/next";

#define HTTP_REQ_MAX_LEN	1024
LOCAL char httpReqBuf[HTTP_REQ_MAX_LEN];

extern struct espconn espConn;

extern char authBasicStr[80];


unsigned char *base64_encode(const unsigned char *src, size_t len, size_t *out_len);
int ICACHE_FLASH_ATTR base64encode(const char *src, int srcLen, char *dst, int dstSize)
{
	unsigned char *tempmem = (char*)os_malloc(80);
	unsigned char *result;
	int len;

	if (!tempmem)
	{
		return 0;
	}

	// fake rom malloc init
	mem_init(tempmem);
	result = base64_encode(src, srcLen, &len);
	if (len > dstSize)
	{
		os_free(tempmem);
		return 0;
	}

	// copy result, filter newlines
	char *pDst = dst;
	while (len > 0)
	{
		if (*result != '\n')
		{
			*pDst = *result;
			pDst++;
		}
		result++;
		len--;
	}
	*pDst = '\0';

	os_free(tempmem);
	return pDst-dst;
}


LOCAL void ICACHE_FLASH_ATTR asciiHex(unsigned char byte, char *ascii)
{
	const char *hex = "0123456789ABCDEF";
	ascii[0] = hex[byte >> 4];
	ascii[1] = hex[byte & 0x0F];
}

LOCAL int ICACHE_FLASH_ATTR charNeedEscape(char ch)
{
	if ((ch >= '0' && ch <= '9') ||
		(ch >= 'A' && ch <= 'Z') ||
		(ch >= 'a' && ch <= 'z') ||
		 ch == '-' || ch == '.'  ||
		 ch == '_' || ch == '~')
	{
		return 0;
	}
	return 1;
}

LOCAL int ICACHE_FLASH_ATTR percentEncode(const char *src, int srcLen, char *dst, int dstSize)
{
	char ch;
	int len = 0;
	while (srcLen--)
	{
		ch = *src++;
		if (charNeedEscape(ch))
		{
			if ((len+2) < dstSize)
			{
				*dst = '%';
				dst++;
				asciiHex(ch, dst);
				dst += 2;
				len += 3;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			if (len < dstSize)
			{
				*dst = ch;
				dst++;
				len++;
			}
			else
			{
				return 0;
			}
		}
	}
	if (len >= dstSize)
	{
		return 0;
	}
	*dst = '\0';
	return len;
}

LOCAL int ICACHE_FLASH_ATTR percentEncodedStrLen(const char *str, int strLen)
{
	int length = 0;
	while (*str && strLen)
	{
		length += charNeedEscape(*str) ? 3 : 1;
		str++;
		strLen--;
	}
	return length;
}


LOCAL ParamItem* ICACHE_FLASH_ATTR paramListAppend(ParamList *list, const char *param, const char *value)
{
	ParamItem *newItem = (ParamItem*)os_malloc(sizeof(ParamItem));
	if (!newItem)
	{
		return NULL;
	}
	newItem->param = param;
	newItem->value = value;
	newItem->valueEncoded = NULL;
	newItem->paramLen = os_strlen(param);
	newItem->valueLen = os_strlen(value);
	newItem->next = NULL;

	if (list->last)
	{
		list->last->next = newItem;
	}
	else	// this is a first item
	{
		list->first = newItem;
	}
	list->last = newItem;
	list->count++;
	return newItem;
}

LOCAL void ICACHE_FLASH_ATTR paramListClear(ParamList *list)
{
	ParamItem *item = list->first;
	ParamItem *next;
	while (item)
	{
		next = item->next;
		os_free(item);
		item = next;
	}
	list->count = 0;
}


LOCAL int ICACHE_FLASH_ATTR paramListStrLen(ParamList *list)
{
	int length = 0;
	ParamItem *item = list->first;
	while (item)
	{
		length += (item->paramLen + item->valueLen + 2);
		item = item->next;
	}
	if (length > 0)
	{
		length--;	// remove last '&'
	}
	return length;
}

LOCAL int paramListEncodeValues(ParamList *list)
{
	ParamItem *param = list->first;
	int len;
	while (param)
	{
		len = percentEncodedStrLen(param->value, param->valueLen);
		param->valueEncoded = (char*)os_malloc(len+1);
		if (!param->valueEncoded)
		{
			return ERROR;
		}
		if (percentEncode(param->value, param->valueLen, param->valueEncoded, len+1) != len)
		{
			return ERROR;
		}
		param->valueLen = len;
		param = param->next;
	}
	return OK;
}

LOCAL void paramListClearEncodedValues(ParamList *list)
{
	ParamItem *param = list->first;
	while (param)
	{
		os_free(param->valueEncoded);
		param->valueEncoded = NULL;
		param = param->next;
	}
}



LOCAL int appendParams(char *dst, int dstSize, const ParamList *paramList)
{
	int len = 0;
	char *pDst = dst;

	ParamItem *param = paramList->first;
	while (param)
	{
		len = ets_snprintf(pDst, dstSize, "%s=%s&", param->param, param->valueEncoded);
		if (len < 0 || len >= dstSize) return 0;
		pDst += len;
		dstSize -= len;
		param = param->next;
	}

	if (len > 0)
	{
		dstSize++;
		pDst--;
		*pDst = '\0';	// remove last '&'
	}
	return pDst - dst;
}

LOCAL int ICACHE_FLASH_ATTR formHttpRequest(char *dst, int dstSize,
		HttpMethod httpMethod, const char *host, const char *endpoint,
		ParamList *paramList, AuthType authType)
{
	int requestLen = 0;
	if (paramListEncodeValues(paramList) != OK)
	{
		goto out;
	}

	const char *method;
	switch (httpMethod)
	{
	case httpGET:
		method = "GET";
		break;
	case httpPOST:
		method = "POST";
		break;
	case httpPUT:
		method = "PUT";
		break;
	default: goto out;
	}

	char *pDst = dst;
	int len = ets_snprintf(pDst, dstSize, "%s %s", method, endpoint);
	if (len < 0 || len >= dstSize) goto out;
	pDst += len;
	dstSize -= len;

	if (httpMethod == httpGET && paramList->count > 0)
	{
		*pDst = '?';
		pDst++;
		dstSize--;
		len = appendParams(pDst, dstSize, paramList);
		if (len == 0) goto out;
		pDst += len;
		dstSize -= len;
	}

	const char *authTypeStr;
	const char *authStr;
	switch (authType)
	{
	case authBasic:
		authTypeStr = "Basic";
		authStr = authBasicStr;
		break;
	case authBearer:
		authTypeStr = "Bearer";
		authStr = config.access_token;
		break;
	default: goto out;
	}

	int contentLen = 0;
	if (httpMethod == httpPOST)
	{
		contentLen = paramListStrLen(paramList);
	}

	len = ets_snprintf(pDst, dstSize,
			" HTTP/1.1\r\n"
			"Accept: */*\r\n"
			//"Connection: close\r\n"
			"Connection: keep-alive\r\n"
			"User-Agent: ESP8266\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Authorization: %s %s\r\n"
			"Content-Length: %d\r\n"
			"Host: %s\r\n\r\n",
			authTypeStr, authStr,
			contentLen, host);
	if (len < 0 || len >= dstSize) goto out;
	pDst += len;
	dstSize -= len;

	if (dstSize < contentLen)
	{
		goto out;
	}

	if (contentLen > 0)
	{
		len = appendParams(pDst, dstSize, paramList);
		if (len == 0) goto out;
		pDst += len;
		dstSize -= len;
	}

	requestLen = pDst - dst;
	debug("\nrequestLen %d\n", requestLen);
	debug("%s\n\n", dst);

out:
	paramListClearEncodedValues(paramList);
	return requestLen;
}


int ICACHE_FLASH_ATTR spotifyRequestTokens(const char *host, const char *code)
{
    int rv = ERROR;
	ParamList params;
	os_memset(&params, 0, sizeof(ParamList));
	paramListAppend(&params, "code", code);
	paramListAppend(&params, "grant_type", "authorization_code");
	paramListAppend(&params, "redirect_uri", "http://httpbin.org/anything");
	int requestLen = formHttpRequest(httpReqBuf, HTTP_REQ_MAX_LEN,
						httpPOST, host, spotifyTokenEndpoint, &params, authBasic);
	if (requestLen > 0)
	{
		if (espconn_secure_send(&espConn, (uint8*)httpReqBuf, requestLen) == OK)
        {
            rv = OK;
        }
	}
	else
	{
		debug("spotifyRequestTokens formHttpRequest failed\n");
	}
	paramListClear(&params);
    return rv;
}

int ICACHE_FLASH_ATTR spotifyRefreshTokens(const char *host, const char *refreshToken)
{
    int rv = ERROR;
	ParamList params;
	os_memset(&params, 0, sizeof(ParamList));
	paramListAppend(&params, "grant_type", "refresh_token");
	paramListAppend(&params, "refresh_token", refreshToken);
	int requestLen = formHttpRequest(httpReqBuf, HTTP_REQ_MAX_LEN,
						httpPOST, host, spotifyTokenEndpoint, &params, authBasic);
	if (requestLen > 0)
	{
		if (espconn_secure_send(&espConn, (uint8*)httpReqBuf, requestLen) == OK)
        {
            rv = OK;
        }
	}
	else
	{
		debug("spotifyRefreshTokens formHttpRequest failed\n");
	}
	paramListClear(&params);
    return rv;
}

int ICACHE_FLASH_ATTR spotifyGetCurrentlyPlaying(const char *host)
{
	int rv = ERROR;
	ParamList params;
	os_memset(&params, 0, sizeof(ParamList));
	int requestLen = formHttpRequest(httpReqBuf, HTTP_REQ_MAX_LEN,
			httpGET, host, spotifyCurrentlyPlayingEndpoint, &params, authBearer);
	if (requestLen > 0)
	{
		if (espconn_secure_send(&espConn, (uint8*)httpReqBuf, requestLen) == OK)
        {
            rv = OK;
        }
	}
	else
	{
		debug("getUserInfo formHttpRequest failed\n");
	}
	paramListClear(&params);
    return rv;
}

int ICACHE_FLASH_ATTR spotifySendPlayerCmd(const char *host, PlayerCmd cmd)
{
	int rv = ERROR;
	ParamList params;
	os_memset(&params, 0, sizeof(ParamList));
	HttpMethod method;
	const char *endPoint = NULL;
	switch (cmd)
	{
	case cmdPlay:
		method = httpPUT;
		endPoint = spotifyPlayerPlayEndpoint;
		break;
	case cmdPause:
		method = httpPUT;
		endPoint = spotifyPlayerPauseEndpoint;
		break;
	case cmdNext:
		method = httpPOST;
		endPoint = spotifyPlayerNextEndpoint;
		break;
	}
	if (endPoint)
	{
		int requestLen = formHttpRequest(httpReqBuf, HTTP_REQ_MAX_LEN,
				method, host, endPoint, &params, authBearer);
		if (requestLen > 0)
		{
			if (espconn_secure_send(&espConn, (uint8*)httpReqBuf, requestLen) == OK)
			{
				rv = OK;
			}
		}
		else
		{
			debug("spotifySendPlayerCmd formHttpRequest failed\n");
		}
	}
	paramListClear(&params);
    return rv;
}


