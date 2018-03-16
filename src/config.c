#include <os_type.h>
#include <osapi.h>
#include <user_interface.h>
#include <spi_flash.h>
#include <limits.h>
#include "config.h"

extern void initAuthBasicStr(void);
extern void connectToAuthHost(void);
char auth_code[400] = "";

Config config;

LOCAL int sysRestart = FALSE;

void ICACHE_FLASH_ATTR configInit(Config *config)
{
	os_memset(config, 0, sizeof(Config));
	config->magic = VALID_MAGIC_NUMBER;
	os_strcpy(config->ssid, DEFAULT_SSID);
	os_strcpy(config->pass, DEFAULT_PASS);

	os_strcpy(config->client_id, DEFAULT_CLIENT_ID);
	os_strcpy(config->client_secret, DEFAULT_CLIENT_SECRET);

	config->access_token[0] = '\0';
	config->refresh_token[0] = '\0';

	config->pollInterval = 10;

	config->tokenExpireTs = 0;

	config->scrollMode = eVHScroll;
	config->showAlbum = TRUE;
    
    config->debugEn = TRUE;
}

void ICACHE_FLASH_ATTR configRead(Config *config)
{
	spi_flash_read(CONFIG_SAVE_FLASH_ADDR, (uint*)config, sizeof(Config));
	if (config->magic != VALID_MAGIC_NUMBER)
	{
		os_printf("no valid config in flash\n");
		configInit(config);
		configWrite(config);
	}
	else
	{
		os_printf("valid config found\n");
	}
}

void ICACHE_FLASH_ATTR configWrite(Config *config)
{
	spi_flash_erase_sector(CONFIG_SAVE_FLASH_SECTOR);
	spi_flash_write(CONFIG_SAVE_FLASH_ADDR, (uint*)config, sizeof(Config));
}

LOCAL int ICACHE_FLASH_ATTR resetConfig(const char *value, uint valueLen)
{
	configInit(&config);
	sysRestart = TRUE;
	return OK;
}


LOCAL int ICACHE_FLASH_ATTR setParam(char *param, uint paramSize, const char *value, uint valueLen)
{
	if (!value || valueLen > paramSize-1)
	{
		return ERROR;
	}
	os_memset(param, 0, paramSize);
	os_memcpy(param, value, valueLen);
	return OK;
}

LOCAL int ICACHE_FLASH_ATTR setBoolParam(int *param, const char *value, uint valueLen)
{
	if (!value || !*value || !valueLen)
		return ERROR;

	switch (value[0])
	{
	case '0':
		*param = FALSE;
		return OK;
	case '1':
		*param = TRUE;
		return OK;
	default:
		return ERROR;
	}
}

LOCAL int ICACHE_FLASH_ATTR setIntParam(int *param, int min, int max, const char *value, uint valueLen)
{
	if (!value || !*value || !valueLen)
		return ERROR;

	int intVal = strtoint(value);
	if (intVal < min || intVal > max)
		return ERROR;

	*param = intVal;
	return OK;
}


LOCAL int ICACHE_FLASH_ATTR setSsid(const char *value, uint valueLen)
{
	if (setParam(config.ssid, sizeof(config.ssid), value, valueLen) == OK)
	{
		sysRestart = TRUE;
		return OK;
	}
	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR setPass(const char *value, uint valueLen)
{
	if (setParam(config.pass, sizeof(config.pass), value, valueLen) == OK)
	{
		sysRestart = TRUE;
		return OK;
	}
	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR setClientId(const char *value, uint valueLen)
{
	if (setParam(config.client_id, sizeof(config.client_id), value, valueLen) != OK)
	{
		return ERROR;
	}
	initAuthBasicStr();
	connectToAuthHost();
	return OK;
}

LOCAL int ICACHE_FLASH_ATTR setClientSecret(const char *value, uint valueLen)
{
	if (setParam(config.client_secret, sizeof(config.client_secret), value, valueLen) != OK)
	{
		return ERROR;
	}
	initAuthBasicStr();
	connectToAuthHost();
	return OK;
}

LOCAL int ICACHE_FLASH_ATTR setAuthCode(const char *value, uint valueLen)
{
	if (setParam(auth_code, sizeof(auth_code), value, valueLen) != OK)
	{
		return ERROR;
	}
	connectToAuthHost();
	return OK;
}

LOCAL int ICACHE_FLASH_ATTR setPollInterval(const char *value, uint valueLen)
{
	return setIntParam(&config.pollInterval, 1, INT_MAX, value, valueLen);
}

LOCAL int ICACHE_FLASH_ATTR setScroll(const char *value, uint valueLen)
{
	return setIntParam((int*)&config.scrollMode, 0, 3, value, valueLen);
}

LOCAL int ICACHE_FLASH_ATTR setShowAlbum(const char *value, uint valueLen)
{
	if (setBoolParam(&config.showAlbum, value, valueLen) == OK)
	{
		sysRestart = TRUE;
		return OK;
	}
	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR setDebug(const char *value, uint valueLen)
{
	return setBoolParam(&config.debugEn, value, valueLen);
}


typedef struct
{
    const char *cmd;
    int (*func)(const char *value, uint valueLen);
}CmdEntry;

CmdEntry commands[] = {
	{"ssid", setSsid},
	{"pass", setPass},
	{"client_id", setClientId},
	{"client_secret", setClientSecret},
	{"auth_code", setAuthCode},
	{"poll", setPollInterval},
	{"scroll", setScroll},
	{"showalbum", setShowAlbum},
	{"debug", setDebug},
	{"reset", resetConfig},
};

void ICACHE_FLASH_ATTR onUartCmdReceived(char* command, int length)
{
	if (length < 5)
		return;

	char *sep = os_strchr(command, ':');
	if (!sep)
		return;
	*sep = '\0';

	char *value = sep+1;
	int valueLen = os_strlen(value);

	uint i;
	uint nrCmds = sizeof(commands)/sizeof(commands[0]);
	for (i = 0; i < nrCmds; i++)
	{
		if (!os_strcmp(command, commands[i].cmd))
		{
			if (commands[i].func(value, valueLen) == OK)
			{
				os_printf("OK\n");
				configWrite(&config);
				if (sysRestart)
				{
					system_restart();
				}
			}
			else
			{
				os_printf("invalid parameter\n");
			}
			return;
		}
	}
	os_printf("command not supported\n");
}
