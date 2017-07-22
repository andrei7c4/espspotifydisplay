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
	httpPOST,
	httpPUT
}HttpMethod;

typedef enum{
	authBasic,
	authBearer
}AuthType;

typedef enum{
	cmdPlay,
	cmdPause,
	cmdNext
}PlayerCmd;


int base64encode(const char *src, int srcLen, char *dst, int dstSize);

int spotifyRequestTokens(const char *host, const char *code);
int spotifyRefreshTokens(const char *host, const char *refreshToken);
int spotifyGetCurrentlyPlaying(const char *host);
int spotifySendPlayerCmd(const char *host, PlayerCmd cmd);

#endif /* SRC_HTTPREQ_H_ */
