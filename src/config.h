#ifndef INCLUDE_CONFIG_H_
#define INCLUDE_CONFIG_H_

#include "typedefs.h"


#define DEFAULT_SSID	""
#define DEFAULT_PASS	""

#define DEFAULT_CLIENT_ID		""
#define DEFAULT_CLIENT_SECRET	""


#define CONFIG_SAVE_FLASH_SECTOR	0x0F
#define CONFIG_SAVE_FLASH_ADDR		(CONFIG_SAVE_FLASH_SECTOR * SPI_FLASH_SEC_SIZE)
#define VALID_MAGIC_NUMBER			0x0ABCDEF0

typedef enum
{
	eNoScroll=0,
	eVScroll,
	eHScroll,
	eVHScroll
}ScrollMode;

typedef struct{
	uint magic;
	char ssid[36];
	char pass[68];

	char client_id[64];
	char client_secret[64];

	char access_token[256];
	char refresh_token[256];
	uint tokenExpireTs;

	int pollInterval;

	ScrollMode scrollMode;
	int showAlbum;
    
    int debugEn;
}Config;
extern Config config;


void configInit(Config *config);
void configRead(Config *config);
void configWrite(Config *config);



#endif /* INCLUDE_CONFIG_H_ */
