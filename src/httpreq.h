#ifndef SRC_HTTPREQ_H_
#define SRC_HTTPREQ_H_

typedef struct ParamItem ParamItem;
typedef struct ParamList ParamList;

struct ParamItem
{
	const char *param;
	const char *value;
	char *valueEncoded;
	int paramLen;
	int valueLen;
	ParamItem *next;
};

struct ParamList
{
	ParamItem *first;
	ParamItem *last;
	int count;
};

typedef enum{
	httpGET,
	httpPOST
}HttpMethod;

typedef enum{
	authBasic,
	authBearer
}AuthType;


int base64encode(const char *src, int srcLen, char *dst, int dstSize);

int spotifyRequestTokens(const char *host, const char *code);
int spotifyRefreshTokens(const char *host, const char *refreshToken);
int spotifyGetCurrentlyPlaying(const char *host);

#endif /* SRC_HTTPREQ_H_ */
