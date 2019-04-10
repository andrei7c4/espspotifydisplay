#include <os_type.h>
#include <osapi.h>
#include <ip_addr.h>
#include <lwip/err.h>
#include <lwip/dns.h>
#include <user_interface.h>
#include <espconn.h>
#include <mem.h>
#include <sntp.h>
#include <gpio.h>
#include "drivers/uart.h"
#include "drivers/spi.h"
#include "drivers/i2c_bitbang.h"
#include "config.h"
#include "debug.h"
#include "httpreq.h"
#include "common.h"
#include "parseobjects.h"
#include "fonts.h"
#include "icons.h"
#include "strlib.h"
#include "graphics.h"
#include "animations.h"
#include "display.h"
#include "SSD1322.h"
#include "SH1106.h"
#include "mpu6500.h"
#include "hwconf.h"



char authBasicStr[100] = "";
extern char auth_code[];

LOCAL TrackInfo curTrack = {0};

LOCAL void ICACHE_FLASH_ATTR trackInfoFree(TrackInfo *track)
{
	os_free(track->name.str);
	strListClear(&track->artists);
	strListClear(&track->album);
}

int ICACHE_FLASH_ATTR curTrackIsPlaying(void)
{
	return curTrack.isPlaying;
}


LOCAL os_timer_t gpTmr;
LOCAL os_timer_t httpRxTmr;
LOCAL os_timer_t pollCurTrackTmr;
LOCAL os_timer_t progressTmr;
LOCAL os_timer_t buttonsTmr;
LOCAL os_timer_t screenSaverTmr;
LOCAL os_timer_t accelTmr;

#define HTTP_RX_BUF_SIZE	8192
char httpMsgRxBuf[HTTP_RX_BUF_SIZE];
int httpRxMsgCurLen = 0;

const uint dnsCheckInterval = 100;

struct espconn espConn = {0};
LOCAL struct _esp_tcp espConnTcp = {0};


LOCAL void requestTokens(void);
LOCAL void refreshTokens(void);
LOCAL void sendApiRequest(void (*requestFunc)(void));
LOCAL void getCurrentlyPlaying(void);
LOCAL void sendPlayPauseCmd(void);
LOCAL void sendNextTrackCmd(void);
LOCAL void handleApiReply(void);
LOCAL void handleAuthReply(void);

typedef struct
{
	const char *host;
	ip_addr_t ip;
	void (*requestFunc)(void);
	void (*handlerFunc)(void);
}ConnParams;
ConnParams authConnParams = {"accounts.spotify.com", {0}, requestTokens, handleAuthReply};
ConnParams apiConnParams = {"api.spotify.com", {0}, getCurrentlyPlaying, handleApiReply};

LOCAL int disconnExpected = FALSE;
LOCAL int reconnCbCalled = FALSE;

LOCAL void connectToWiFiAP(void);
LOCAL uint8 getWiFiStatusAndIp(uint32 *addr);
LOCAL void checkWiFiConnStatus(void);
LOCAL void checkSntpSync(void);
LOCAL void connectToHost(ConnParams *params);
LOCAL void connectFirstTime(ConnParams *params);
LOCAL void checkDnsStatus(void *arg);
LOCAL void getHostByNameCb(const char *name, ip_addr_t *ipaddr, void *arg);
LOCAL void onTcpConnected(void *arg);
LOCAL void onTcpDataSent(void *arg);
LOCAL void onTcpDataRecv(void *arg, char *pusrdata, unsigned short length);
LOCAL void onTcpDisconnected(void *arg);
LOCAL void reconnect(void);
LOCAL void onTcpReconnCb(void *arg, sint8 err);
LOCAL void pollCurTrackTmrCb(void);
LOCAL void progressTmrCb(void);
LOCAL void buttonsScanTmrCb(void);
LOCAL void drawSpotifyLogo(void);
LOCAL void wakeupDisplay(void);
LOCAL void screenSaverTmrCb(void);
LOCAL void accelTmrCb(void);


typedef enum{
	stateInit,
	stateConnectToAp,
    stateConnectToHost,
    stateConnected,
	stateRequestSent,
	stateReplyReceived
}AppState;
AppState appState = stateInit;

LOCAL void ICACHE_FLASH_ATTR setAppState(AppState newState)
{
	if (appState != newState)
	{
		appState = newState;
		debug("appState %d\n", (int)appState);
	}
}


typedef enum{
	NotPressed,
	Button1,
	Button2
}Button;

#if defined BUTTONS_ON_ADC
LOCAL Button ICACHE_FLASH_ATTR adcValToButton(uint16 adcVal)
{
	if (adcVal >= 768) return NotPressed;
	if (adcVal >= 256) return Button1;
	return Button2;
}
#elif defined BUTTON_ON_GPIO0
LOCAL Button ICACHE_FLASH_ATTR readGpio0ButtonState(void)
{
	static int pressed = 0;
	Button state = NotPressed;
	if (GPIO_INPUT_GET(0) == 0)
	{
		pressed++;
	}
	else if (pressed > 0)
	{
		if (pressed >= 10)	// pressed for >= 1s
		{
			state = Button2;
		}
		else
		{
			state = Button1;
		}
		pressed = 0;
	}
	return state;
}
#endif


void ICACHE_FLASH_ATTR initAuthBasicStr(void)
{
	if (!config.client_id[0] || !config.client_secret[0])
	{
		return;
	}

	int tempSize = sizeof(config.client_id) + sizeof(config.client_secret) + 2;
	char *temp = (char*)os_malloc(tempSize);
	int len = ets_snprintf(temp, tempSize, "%s:%s", config.client_id, config.client_secret);
	if (len == 0 || len >= tempSize) return;
	if (!base64encode(temp, len, authBasicStr, sizeof(authBasicStr)))
	{
		authBasicStr[0] = '\0';
		debug("base64encode failed\n");
	}
	os_free(temp);
	//debug("authBasicStr: %s\n", authBasicStr);
}


int ICACHE_FLASH_ATTR dispInitDone(void)
{
	debug("dispInitDone\n");
	drawSpotifyLogo();
	dispUpdateFull();
	wakeupDisplay();
	return OK;
}


void ICACHE_FLASH_ATTR user_init(void)
{
	os_timer_disarm(&gpTmr);
	os_timer_disarm(&httpRxTmr);

	os_timer_disarm(&pollCurTrackTmr);
	os_timer_setfn(&pollCurTrackTmr, (os_timer_func_t*)pollCurTrackTmrCb, NULL);

	os_timer_disarm(&progressTmr);
	os_timer_setfn(&progressTmr, (os_timer_func_t*)progressTmrCb, NULL);

	os_timer_disarm(&buttonsTmr);
	os_timer_setfn(&buttonsTmr, (os_timer_func_t*)buttonsScanTmrCb, NULL);

	os_timer_disarm(&screenSaverTmr);
	os_timer_setfn(&screenSaverTmr, (os_timer_func_t*)screenSaverTmrCb, NULL);

	os_timer_disarm(&accelTmr);
	os_timer_setfn(&accelTmr, (os_timer_func_t*)accelTmrCb, NULL);

	//uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart_init(BIT_RATE_921600, BIT_RATE_921600);

	activeBuf = &MainGfxBuf;
	activeBufClearAll();

//configInit(&config);
//configWrite(&config);
	configRead(&config);

	initAuthBasicStr();
	initPaths();

	debug("Built on %s %s\n", __DATE__, __TIME__);
	debug("SDK version %s\n", system_get_sdk_version());
	debug("free heap %d\n", system_get_free_heap_size());

	os_memset(&curTrack, 0, sizeof(TrackInfo));
	curTrack.name.str = (ushort*)os_malloc(sizeof(ushort));
	curTrack.name.str[0] = 0;

	os_memset(&TitleLabel, 0, sizeof(Label));
	os_memset(&ArtistLabel, 0, sizeof(Label));
	os_memset(&AlbumLabel, 0, sizeof(Label));

#if DISP_HEIGHT < 64	// there's no space left for album on smallest displays
	config.showAlbum = FALSE;
#endif
	setLabelDimensions(config.showAlbum);


	gpio_init();

#if (DISP_TYPE == 1322) || defined (MPU_ON_SPI)
	spi_init(HSPI, 20, 5, FALSE);	// spi clock = 800 kHz

	// same spi settings for SSD1322 and MPU6500
	// data is valid on clock trailing edge
	// clock is high when inactive
	spi_mode(HSPI, 1, 1);
#endif

#if defined (MPU_ON_SPI)
	if (mpu6500_init() == OK)
	{
		// accelerometer found -> read values twice per second
		os_timer_arm(&accelTmr, 500, 1);
	}
	else
	{
		debug("mpu6500_init failed\n");
	}
#endif

#if DISP_TYPE == 1322
	SSD1322_init(orient0deg, FALSE);
	dispInitDone();
#elif DISP_TYPE == 1106
	i2c_gpio_init();
	SH1106_init(TRUE, dispInitDone, orient0deg, FALSE);
#elif DISP_TYPE == 1306
	i2c_gpio_init();
	SSD1306_init(TRUE, dispInitDone, orient180deg, FALSE);
#endif

		//wifi_set_opmode(NULL_MODE);
		//return;

	setAppState(stateConnectToAp);
	wifi_set_opmode(STATION_MODE);
	connectToWiFiAP();

#if defined BUTTONS_ON_ADC
	if (adcValToButton(system_adc_read()) == NotPressed)
	{
		// enable buttons scan
		os_timer_arm(&buttonsTmr, 100, 1);
	}
	//else pull-ups are probably not installed -> don't scan buttons
#elif defined BUTTON_ON_GPIO0
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	GPIO_DIS_OUTPUT(0);

	os_timer_arm(&buttonsTmr, 100, 1);
#endif
}

LOCAL void ICACHE_FLASH_ATTR connectToWiFiAP(void)
{
	struct station_config stationConf;
	stationConf.bssid_set = 0;	// mac address not needed
	os_memcpy(stationConf.ssid, config.ssid, sizeof(stationConf.ssid));
	os_memcpy(stationConf.password, config.pass, sizeof(stationConf.password));
	wifi_station_set_config(&stationConf);

	checkWiFiConnStatus();
}

LOCAL uint8 ICACHE_FLASH_ATTR getWiFiStatusAndIp(uint32 *addr)
{
	struct ip_info ipconfig;
	os_memset(&ipconfig, 0, sizeof(ipconfig));
	uint8 status = wifi_station_get_connect_status();
	if (status == STATION_GOT_IP)
	{
		wifi_get_ip_info(STATION_IF, &ipconfig);
	}
	*addr = ipconfig.ip.addr;
	return status;
}

LOCAL void ICACHE_FLASH_ATTR checkWiFiConnStatus(void)
{
	// check current connection status and own ip address
	uint32 ipAddr;
	uint8 connStatus = getWiFiStatusAndIp(&ipAddr);
	if (connStatus == STATION_GOT_IP && ipAddr != 0)
	{
		// connection with AP established -> sync time
        // TODO: make addresses configurable
		sntp_setservername(0, "europe.pool.ntp.org");
		sntp_setservername(1, "us.pool.ntp.org");
		sntp_set_timezone(0);
		sntp_init();
		checkSntpSync();
	}
	else
	{
		if (connStatus == STATION_WRONG_PASSWORD ||
			connStatus == STATION_NO_AP_FOUND	 ||
			connStatus == STATION_CONNECT_FAIL)
		{
			debug("Failed to connect to AP (status: %u)\n", connStatus);
		}
		else
		{
			// not yet connected, recheck later
			os_timer_setfn(&gpTmr, (os_timer_func_t*)checkWiFiConnStatus, NULL);
			os_timer_arm(&gpTmr, 100, 0);
		}
	}
}

void connectToAuthHost(void);
LOCAL void ICACHE_FLASH_ATTR checkSntpSync(void)
{
	uint ts = sntp_get_current_timestamp();
	if (ts < 1483228800)	// 1.1.2017
	{
		// not yet synced, recheck later
		os_timer_setfn(&gpTmr, (os_timer_func_t*)checkSntpSync, NULL);
		os_timer_arm(&gpTmr, 100, 0);
		return;
	}

	// time synced -> connect to Spotify
	if (config.access_token[0] && config.refresh_token[0])
	{
		if ((ts+5) < config.tokenExpireTs)
		{
			connectToHost(&apiConnParams);
		}
		else
		{
			authConnParams.requestFunc = refreshTokens;
			connectToHost(&authConnParams);
		}
	}
	else
	{
		connectToAuthHost();
	}
}

LOCAL void ICACHE_FLASH_ATTR connectToHost(ConnParams *params)
{
	uint32 ipAddr;
	uint8 connStatus = getWiFiStatusAndIp(&ipAddr);
	if (connStatus != STATION_GOT_IP || !ipAddr)
	{
		return;
	}
    setAppState(stateConnectToHost);
        
	espConn.reverse = params;
	if (espConn.state == ESPCONN_CONNECT || 	// if we are currently connected to some host -> disconnect
		espConn.state == ESPCONN_READ)			// and try to connect when disconnection occurs
	{
		disconnExpected = TRUE;
		// disconnect should not be called directly from here
		os_timer_setfn(&gpTmr, (os_timer_func_t*)espconn_secure_disconnect, &espConn);
		os_timer_arm(&gpTmr, 100, 0);
	}
	else	// we are currently not connected to any host
	{
		if (params->ip.addr)	// we have ip of the host
		{
			// use this ip and try to connect
			os_memcpy(&espConn.proto.tcp->remote_ip, &params->ip.addr, 4);
			reconnect();
		}
		else	// we don't yet have ip of the host
		{
			connectFirstTime(params);
		}
	}
}

void ICACHE_FLASH_ATTR connectToAuthHost(void)	// called from config.c
{
	if (appState >= stateConnectToAp && authBasicStr[0] && auth_code[0])
	{
		authConnParams.requestFunc = requestTokens;
		connectToHost(&authConnParams);
	}
}

LOCAL void ICACHE_FLASH_ATTR connectFirstTime(ConnParams *params)
{
	// connection with AP established -> get server ip
	espConn.proto.tcp = &espConnTcp;
	espConn.type = ESPCONN_TCP;
	espConn.state = ESPCONN_NONE;
	espConn.reverse = params;

	// register callbacks
	espconn_regist_connectcb(&espConn, onTcpConnected);
	espconn_regist_reconcb(&espConn, onTcpReconnCb);

	espconn_gethostbyname(&espConn, params->host, &params->ip, getHostByNameCb);

	os_timer_setfn(&gpTmr, (os_timer_func_t*)checkDnsStatus, params);
	os_timer_arm(&gpTmr, dnsCheckInterval, 0);
}

LOCAL void ICACHE_FLASH_ATTR checkDnsStatus(void *arg)
{
	ConnParams *params = arg;
	espconn_gethostbyname(&espConn, params->host, &params->ip, getHostByNameCb);
	os_timer_arm(&gpTmr, dnsCheckInterval, 0);
}

LOCAL void ICACHE_FLASH_ATTR getHostByNameCb(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;
    ConnParams *params = pespconn->reverse;
	os_timer_disarm(&gpTmr);

	if (params->ip.addr != 0)
	{
		debug("getHostByNameCb serverIp != 0\n");
		return;
	}
	if (ipaddr == NULL || ipaddr->addr == 0)
	{
		debug("getHostByNameCb ip NULL\n");
		return;
	}
	debug("getHostByNameCb ip: "IPSTR"\n", IP2STR(ipaddr));
    
	// connect to host
	params->ip.addr = ipaddr->addr;
	os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);
	pespconn->proto.tcp->remote_port = 443;	// use HTTPS port
	pespconn->proto.tcp->local_port = espconn_port();	// get next free local port number
	
	espconn_secure_set_size(ESPCONN_CLIENT, 8192);
	int rv = espconn_secure_connect(pespconn);	// tcp SSL connect
	debug("espconn_secure_connect %d\n", rv);

	os_timer_setfn(&gpTmr, (os_timer_func_t*)reconnect, 0);
	os_timer_arm(&gpTmr, 10000, 0);
}

LOCAL void ICACHE_FLASH_ATTR onTcpConnected(void *arg)
{
    setAppState(stateConnected);
    
	debug("onTcpConnected\n");
	struct espconn *pespconn = arg;
	ConnParams *params = pespconn->reverse;
	os_timer_disarm(&gpTmr);
	// register callbacks
	espconn_regist_recvcb(pespconn, onTcpDataRecv);
	espconn_regist_sentcb(pespconn, onTcpDataSent);
	espconn_regist_disconcb(pespconn, onTcpDisconnected);

	params->requestFunc();
	reconnCbCalled = FALSE;
}

LOCAL void ICACHE_FLASH_ATTR onTcpDataSent(void *arg)
{
	debug("onTcpDataSent\n");
}

LOCAL void ICACHE_FLASH_ATTR onTcpDataRecv(void *arg, char *pusrdata, unsigned short length)
{
	debug("onTcpDataRecv %d\n", length);
	os_timer_disarm(&httpRxTmr);
	
	ConnParams *params = espConn.reverse;
	int spaceLeft = (HTTP_RX_BUF_SIZE-httpRxMsgCurLen-1);
	int bytesToCopy = MIN(spaceLeft, length);
	os_memcpy(httpMsgRxBuf+httpRxMsgCurLen, pusrdata, bytesToCopy);
	httpRxMsgCurLen += bytesToCopy;
	if (httpRxMsgCurLen > 0)
	{
		if (httpRxMsgCurLen < (HTTP_RX_BUF_SIZE-1))
		{			
			os_timer_setfn(&httpRxTmr, (os_timer_func_t*)params->handlerFunc, NULL);
			os_timer_arm(&httpRxTmr, 100, 0);
		}
		else
		{
			debug("httpMsgRxBuf full\n");
			params->handlerFunc();
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR onTcpDisconnected(void *arg)
{
	// on unexpected disconnection the following might happen:
	// usually: only onTcpReconnCb is called
	// sometimes: only onTcpDisconnected is called
	// rarely: both onTcpReconnCb and onTcpDisconnected are called

	struct espconn *pespconn = arg;
	ConnParams *params = pespconn->reverse;
	debug("onTcpDisconnected\n");
	debug("disconnExpected %d\n", disconnExpected);
	if (!disconnExpected)	// we got unexpectedly disconnected
	{
		debug("reconnCbCalled %d\n", reconnCbCalled);
		if (reconnCbCalled)		// if onTcpReconnCb was also called
		{						// just ignore this callback
			reconnCbCalled = FALSE;
			return;
		}

		// reconnect automatically to API host only
		if (params != &apiConnParams)
		{
			return;
		}

		// server probably closed connection due to inactivity
		// no need to reconnect immediately, will reconnect from pollCurTrackTmrCb
		if (appState == stateReplyReceived)
		{
			return;
		}
	}
	else
	{
		disconnExpected = FALSE;
	}

	// in all other cases -> try to reconnect
	if (params->ip.addr)	// we have ip of the host
	{
		// use this ip and try to connect
		os_memcpy(&espConn.proto.tcp->remote_ip, &params->ip.addr, 4);
		reconnect();
	}
	else	// we don't yet have ip of the host
	{
		connectFirstTime(params);
	}
}

LOCAL void ICACHE_FLASH_ATTR onTcpReconnCb(void *arg, sint8 err)
{
	debug("onTcpReconnCb\n");

	// ok, something went wrong and we got disconnected
	// try to reconnect in 5 sec
	os_timer_disarm(&gpTmr);
	os_timer_setfn(&gpTmr, (os_timer_func_t*)reconnect, 0);
	os_timer_arm(&gpTmr, 5000, 0);
	reconnCbCalled = TRUE;
}

LOCAL void ICACHE_FLASH_ATTR reconnect(void)
{
	debug("reconnect\n");
	int rv = espconn_secure_connect(&espConn);
	debug("espconn_secure_connect %d\n", rv);

	os_timer_disarm(&gpTmr);
	os_timer_setfn(&gpTmr, (os_timer_func_t*)reconnect, 0);
	os_timer_arm(&gpTmr, 10000, 0);
}



LOCAL void ICACHE_FLASH_ATTR pollCurTrackTmrCb(void)
{
	sendApiRequest(getCurrentlyPlaying);
}


LOCAL void ICACHE_FLASH_ATTR updateTrackProgress(int progress, int duration)
{
	activeBuf = &MainGfxBuf;
	activeBufClearProgBar();

#if PROGBAR_HEIGHT >= 10
	char timeStr[7];
	int minutes = progress / 60;
	int seconds = progress % 60;
	int len = ets_snprintf(timeStr, sizeof(timeStr), "%d:%02d", minutes, seconds);
	drawStr_Latin(&sevensegment, 0, PROGBAR_OFFSET, timeStr, len);

	minutes = duration / 60;
	seconds = duration % 60;
	len = ets_snprintf(timeStr, sizeof(timeStr), "%d:%02d", minutes, seconds);
	int timeStrWidth = drawStrAlignRight_Latin(&sevensegment, DISP_WIDTH-1, PROGBAR_OFFSET, timeStr, len);

	const int barX = timeStrWidth + 7;
	const int barMaxWidth = DISP_WIDTH-1 - (barX*2);
	const int barXmax = barX+barMaxWidth-1;
	const int barHeight = 3;
	const int barY = PROGBAR_OFFSET + 6;
	const int barY2 = barY + barHeight - 1;
	drawLine(barX, barY-1, barXmax, barY-1, 1);
	drawLine(barX, barY2+1, barXmax, barY2+1, 1);
	drawLine(barX-1, barY, barX-1, barY2, 1);
	drawLine(barXmax+1, barY, barXmax+1, barY2, 1);

	float progressPercent = ((float)progress) / duration;
	int barWidth = progressPercent * barMaxWidth + 0.5;
	if (barWidth > 0)
	{
		int barX2 = barX + barWidth - 1;
		drawRect(barX, barY, barX2, barY2, 1);
	}
#elif PROGBAR_HEIGHT >= 1
	float progressPercent = ((float)progress) / duration;
	int barWidth = progressPercent * DISP_WIDTH + 0.5;
	if (barWidth > 0)
	{
		drawLine(0, PROGBAR_OFFSET, barWidth, PROGBAR_OFFSET, 1);
	}
#endif

	dispUpdateProgBar();
}

LOCAL void ICACHE_FLASH_ATTR progressTmrCb(void)
{
	if (curTrack.progress < curTrack.duration)
	{
		curTrack.progress++;
		updateTrackProgress(curTrack.progress, curTrack.duration);
	}
	else
	{
		os_timer_disarm(&progressTmr);
	}
}


LOCAL void ICACHE_FLASH_ATTR authRequestRetry(void)
{
	// only ESPCONN_CONNECT state is good for sending data
	if (espConn.state == ESPCONN_CONNECT && espConn.reverse == &authConnParams)
	{
		authConnParams.requestFunc();
	}
	else	// in all other cases we need to reconnect
	{
		connectToHost(&authConnParams);
	}
}

LOCAL void ICACHE_FLASH_ATTR requestTokens(void)
{
    if (spotifyRequestTokens(authConnParams.host, auth_code) == OK)
    {
        setAppState(stateRequestSent);

        // if no answer, retry after 5s
		os_timer_disarm(&gpTmr);
		os_timer_setfn(&gpTmr, (os_timer_func_t*)authRequestRetry, 0);
		os_timer_arm(&gpTmr, 5000, 0);
    }
}

LOCAL void ICACHE_FLASH_ATTR refreshTokens(void)
{
	if (spotifyRefreshTokens(authConnParams.host, config.refresh_token) == OK)
	{
		setAppState(stateRequestSent);

        // if no answer, retry after 5s
		os_timer_disarm(&gpTmr);
		os_timer_setfn(&gpTmr, (os_timer_func_t*)authRequestRetry, 0);
		os_timer_arm(&gpTmr, 5000, 0);
	}
}

LOCAL void ICACHE_FLASH_ATTR sendApiRequest(void (*requestFunc)(void))
{
	apiConnParams.requestFunc = requestFunc;
	uint ts = sntp_get_current_timestamp();
	if ((ts+5) > config.tokenExpireTs)
	{
		authConnParams.requestFunc = refreshTokens;
		connectToHost(&authConnParams);
	}
	else
	{
		// only ESPCONN_CONNECT state is good for sending data
		if (espConn.state == ESPCONN_CONNECT && espConn.reverse == &apiConnParams)
		{
			requestFunc();
		}
		else	// in all other cases we need to reconnect
		{
			connectToHost(&apiConnParams);
		}
	}
}

// call this function through sendApiRequest
LOCAL void ICACHE_FLASH_ATTR getCurrentlyPlaying(void)
{
    if (spotifyGetCurrentlyPlaying(apiConnParams.host) == OK)
    {
        setAppState(stateRequestSent);
    }

    // if no answer, retry after 5s
    os_timer_arm(&pollCurTrackTmr, 5000, 0);
}

// call this function through sendApiRequest
LOCAL void ICACHE_FLASH_ATTR sendPlayPauseCmd(void)
{
	int rv;
	if (curTrack.isPlaying)
	{
		rv = spotifySendPlayerCmd(apiConnParams.host, cmdPause);
	}
	else
	{
		rv = spotifySendPlayerCmd(apiConnParams.host, cmdPlay);
	}
	if (rv == OK)
	{
		setAppState(stateRequestSent);
	}
}

// call this function through sendApiRequest
LOCAL void ICACHE_FLASH_ATTR sendNextTrackCmd(void)
{
	if (spotifySendPlayerCmd(apiConnParams.host, cmdNext))
	{
		setAppState(stateRequestSent);
	}
}




LOCAL void ICACHE_FLASH_ATTR handleAuthReply(void)
{
	setAppState(stateReplyReceived);
	os_timer_disarm(&gpTmr);

	debug("parseAuthReply, len %d\n", httpRxMsgCurLen);
	httpMsgRxBuf[httpRxMsgCurLen] = '\0';
	//debug("%s\n", httpMsgRxBuf);

	if (authConnParams.requestFunc == requestTokens ||
		authConnParams.requestFunc == refreshTokens)
	{
		char *json = (char*)os_strstr(httpMsgRxBuf, "{\"");
		if (!json)
		{
			return;
		}
		int jsonLen = httpRxMsgCurLen - (json - httpMsgRxBuf);
		//debug("jsonLen %d\n", jsonLen);

		Tokens tokens = {config.access_token, sizeof(config.access_token),
						 config.refresh_token, sizeof(config.refresh_token), 0, 0};
	    TokensParsed tokensParsed = parseTokens(json, jsonLen, &tokens);
	    if((tokensParsed & eTokensAccessParsed) && (tokensParsed & eTokensExpiresInParsed))
	    {
			uint ts = sntp_get_current_timestamp();
			config.tokenExpireTs = ts + tokens.expiresIn;
			configWrite(&config);

			connectToHost(&apiConnParams);
	    }
	    else
	    {
			debug("parseTokens failed (0x%02X)\n", tokens.parsed);
		}
	}
	
	httpRxMsgCurLen = 0;
}

LOCAL void ICACHE_FLASH_ATTR handleApiReply(void)
{
	static int firstReply = TRUE;
	setAppState(stateReplyReceived);

	debug("parseApiReply, len %d\n", httpRxMsgCurLen);
	httpMsgRxBuf[httpRxMsgCurLen] = '\0';
	//debug("%s\n", httpMsgRxBuf);

	if (apiConnParams.requestFunc == getCurrentlyPlaying)
	{
		char *json = (char*)os_strstr(httpMsgRxBuf, "{");
		if (json)
		{
			int jsonLen = httpRxMsgCurLen - (json - httpMsgRxBuf);
			//debug("jsonLen %d\n", jsonLen);

			TrackInfo track;
			os_memset(&track, 0, sizeof(TrackInfo));
			if (parseTrackInfo(json, jsonLen, &track) == eTrackAllParsed)
			{
				int trackChanged = (wstrcmp(curTrack.name.str, track.name.str) != 0);
				int artistChanged = !strListEqual(&curTrack.artists, &track.artists);
				int albumChanged = !strListEqual(&curTrack.album, &track.album);

				if (trackChanged)
				{
					debug("new track\n");

					GfxBufAlloc(&TitleLabel.buf, strWidth(TitleLabel.font, track.name.str));
					activeBuf = &TitleLabel.buf;
					drawStr(TitleLabel.font, 0, 0, track.name.str, track.name.length);
				}
				if (artistChanged)
				{
					debug("new artist\n");

					GfxBufAlloc(&ArtistLabel.buf, strListWidth(ArtistLabel.font, &track.artists, L", "));
					activeBuf = &ArtistLabel.buf;
					strListDraw(ArtistLabel.font, 0, 0, &track.artists, L", ");
				}
				if (albumChanged)
				{
					debug("new album\n");

					GfxBufAlloc(&AlbumLabel.buf, strListWidth(AlbumLabel.font, &track.album, L" | "));
					activeBuf = &AlbumLabel.buf;
					strListDraw(AlbumLabel.font, 1, 0, &track.album, L" | ");
				}

				if (trackChanged || artistChanged || albumChanged)
				{
					if (firstReply)
					{
						firstReply = FALSE;
						dispClearBlankSpace(config.showAlbum);
					}

					if (config.scrollMode & eVScroll)
					{
						// start vertical scroll for changed labels
						vScrollStart(trackChanged, artistChanged, albumChanged);
					}
					else	// vertical scroll disabled
					{		// just draw changed labels
						scrollStop();
						if (trackChanged)
						{
							GfxBufCopy(&MainGfxBuf, &TitleLabel.buf, TitleLabel.offset);
						}
						if (artistChanged)
						{
							GfxBufCopy(&MainGfxBuf, &ArtistLabel.buf, ArtistLabel.offset);
						}
						if (albumChanged)
						{
							GfxBufCopy(&MainGfxBuf, &AlbumLabel.buf, AlbumLabel.offset);
						}
						dispUpdateLabels(trackChanged, artistChanged, albumChanged);

						if (config.scrollMode & eHScroll)
						{
							// check if we are interrupting horizontal scroll
							TitleLabel.scrollInt = (TitleLabel.scrollEn && !trackChanged);
							ArtistLabel.scrollInt = (ArtistLabel.scrollEn && !artistChanged);
							AlbumLabel.scrollInt = (AlbumLabel.scrollEn && !albumChanged);

							// start/continue horizontal scroll if it is enabled and label width > display width
							hScrollEnable();
						}
					}

					wakeupDisplay();
				}
				else	// all the same
				{
					debug("same track\n");
					if (!curTrack.isPlaying && track.isPlaying)		// playback resumed
					{
						wakeupDisplay();
						if (config.scrollMode & eHScroll)
						{
							hScrollEnable();
						}
					}
				}

				track.progress /= 1000;		// no need for ms precision
				track.duration /= 1000;
				updateTrackProgress(track.progress, track.duration);
				os_timer_disarm(&progressTmr);
				if (track.isPlaying)
				{
					os_timer_arm(&progressTmr, 1000, 1);
				}

				trackInfoFree(&curTrack);
				curTrack = track;
			}
			else
			{
				debug("parseTrack failed (0x%02X)\n", track.parsed);
				trackInfoFree(&track);
			}
		}
		else
		{
			debug("track info not available\n");
			curTrack.isPlaying = FALSE;
		}

		os_timer_disarm(&pollCurTrackTmr);
		int nextPoll;
		if (curTrack.isPlaying)
		{
			// at the end of track, send next poll when track changes
			int timeLeft = curTrack.duration - curTrack.progress;
			nextPoll = MIN(timeLeft+1, config.pollInterval);
		}
		else
		{
			nextPoll = config.pollInterval;
		}
		debug("nextPoll after %d s\n", nextPoll);
		os_timer_arm(&pollCurTrackTmr, nextPoll*1000, 0);
		debug("heap: %u\n", system_get_free_heap_size());
	}
	else if (apiConnParams.requestFunc == sendPlayPauseCmd ||
			 apiConnParams.requestFunc == sendNextTrackCmd)
	{
		char *ok = (char*)os_strstr(httpMsgRxBuf, "HTTP/1.1 204");
		if (ok)
		{
			// command succeeded -> poll currently playing
			os_timer_disarm(&pollCurTrackTmr);
			os_timer_arm(&pollCurTrackTmr, 100, 0);
		}
	}

	httpRxMsgCurLen = 0;
}



LOCAL void ICACHE_FLASH_ATTR buttonsScanTmrCb(void)
{
	static Button prevButtons = NotPressed;
	Button buttons = NotPressed;
#if defined BUTTONS_ON_ADC
	uint16 adcVal = system_adc_read();
	buttons = adcValToButton(adcVal);
	//debug("adcVal %d, buttons %d\n", adcVal, buttons);
#elif defined BUTTON_ON_GPIO0
	buttons = readGpio0ButtonState();
#endif
	if (prevButtons != buttons)
	{
		prevButtons = buttons;
		if (buttons != NotPressed)
		{
			switch (buttons)
			{
			case Button1:
				sendApiRequest(sendPlayPauseCmd);
				break;
			case Button2:
				sendApiRequest(sendNextTrackCmd);
				break;
			}
			wakeupDisplay();
		}
	}
}


// TODO: make times configurable
LOCAL void ICACHE_FLASH_ATTR wakeupDisplay(void)
{
	dispUndimmStart();
	os_timer_arm(&screenSaverTmr, 5*60*1000, 0);
}

LOCAL void ICACHE_FLASH_ATTR screenSaverTmrCb(void)
{
	switch (displayState)
	{
	case stateOn:
		dispDimmingStart();
		os_timer_arm(&screenSaverTmr, 15*60*1000, 0);
		break;
	case stateDimmed:
		if (curTrack.isPlaying)
		{
			// do not turn off the display while track is playing
			os_timer_arm(&screenSaverTmr, 5*60*1000, 0);
		}
		else
		{
			dispSmoothTurnOff();
		}
		break;
	}
}


LOCAL void ICACHE_FLASH_ATTR accelTmrCb(void)
{
	sint16 x = accelReadX();
	if (x < -8192 && dispOrient != orient180deg)
	{
		dispSetOrientation(orient180deg);
	}
	else if (x > 8192 && dispOrient != orient0deg)
	{
		dispSetOrientation(orient0deg);
	}
}


LOCAL void ICACHE_FLASH_ATTR drawSpotifyLogo(void)
{
	int width = spotifyLogo[0];
	int x = (DISP_WIDTH/2) - (width/2);
	drawImage(x, 0, spotifyLogo);
}

