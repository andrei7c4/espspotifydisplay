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
#include "config.h"
#include "debug.h"
#include "httpreq.h"
#include "common.h"
#include "parsejson.h"
#include "fonts.h"
#include "icons.h"
#include "strlib.h"
#include "graphics.h"
#include "display.h"
#include "mpu6500.h"



char authBasicStr[100] = "";
extern char auth_code[];

TrackInfo curTrack = {{0,0},{0,0},0,0,0};

LOCAL void ICACHE_FLASH_ATTR trackInfoFree(TrackInfo *track)
{
	os_free(track->name);
	strListClear(&track->artists);
}


LOCAL os_timer_t gpTmr;
LOCAL os_timer_t httpRxTmr;
LOCAL os_timer_t pollCurTrackTmr;
LOCAL os_timer_t progressTmr;
LOCAL os_timer_t buttonsTmr;
LOCAL os_timer_t screenSaverTmr;
LOCAL os_timer_t accelTmr;
extern os_timer_t scrollTmr;

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
LOCAL void parseApiReply(void);
LOCAL void parseAuthReply(void);

typedef struct
{
	const char *host;
	ip_addr_t ip;
	void (*requestFunc)(void);
	void (*parserFunc)(void);
}ConnParams;
ConnParams authConnParams = {"accounts.spotify.com", {0}, requestTokens, parseAuthReply};
ConnParams apiConnParams = {"api.spotify.com", {0}, getCurrentlyPlaying, parseApiReply};

LOCAL int disconnExpected = FALSE;
LOCAL int reconnCbCalled = FALSE;

LOCAL void connectToWiFiAP(void);
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


void user_init(void)
{
	os_timer_disarm(&gpTmr);
	os_timer_disarm(&httpRxTmr);

	os_timer_disarm(&pollCurTrackTmr);
	os_timer_setfn(&pollCurTrackTmr, (os_timer_func_t*)pollCurTrackTmrCb, NULL);

	os_timer_disarm(&progressTmr);
	os_timer_setfn(&progressTmr, (os_timer_func_t*)progressTmrCb, NULL);

	os_timer_disarm(&scrollTmr);
	os_timer_disarm(&buttonsTmr);
	os_timer_setfn(&buttonsTmr, (os_timer_func_t*)buttonsScanTmrCb, NULL);

	os_timer_disarm(&screenSaverTmr);
	os_timer_setfn(&screenSaverTmr, (os_timer_func_t*)screenSaverTmrCb, NULL);

	os_timer_disarm(&accelTmr);
	os_timer_setfn(&accelTmr, (os_timer_func_t*)accelTmrCb, NULL);

	//uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart_init(BIT_RATE_921600, BIT_RATE_921600);

	dispSetActiveMemBuf(MainMemBuf);
	dispClearMem(0, DISP_HEIGHT);

//configInit(&config);
//configWrite(&config);
	configRead(&config);

	initAuthBasicStr();
	
	curTrack.name.str = (ushort*)os_malloc(sizeof(ushort));
	curTrack.name.str[0] = 0;

	debug("Built on %s %s\n", __DATE__, __TIME__);
	debug("SDK version %s\n", system_get_sdk_version());
	debug("free heap %d\n", system_get_free_heap_size());
    
	gpio_init();

	spi_init(HSPI, 20, 5, FALSE);	// spi clock = 800 kHz

	// same spi settings for SSD1322 and MPU6500
	// data is valid on clock trailing edge
	// clock is high when inactive
	spi_mode(HSPI, 1, 1);

//	if (mpu6500_init() == OK)
//	{
//		// accelerometer found -> read values twice per second
//		os_timer_arm(&accelTmr, 500, 1);
//	}

	SSD1322_init();

	//drawSpotifyLogo();
	dispClearMemAll();
	dispUpdateFull();
	wakeupDisplay();
	
		//wifi_set_opmode(NULL_MODE);
		//return;

	setAppState(stateConnectToAp);
	wifi_set_opmode(STATION_MODE);
	connectToWiFiAP();
    
	// enable buttons scan
	os_timer_arm(&buttonsTmr, 100, 1);
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

LOCAL void ICACHE_FLASH_ATTR checkWiFiConnStatus(void)
{
	struct ip_info ipconfig;
	memset(&ipconfig, 0, sizeof(ipconfig));

	// check current connection status and own ip address
	wifi_get_ip_info(STATION_IF, &ipconfig);
	uint8 connStatus = wifi_station_get_connect_status();
	if (connStatus == STATION_GOT_IP && ipconfig.ip.addr != 0)
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
			os_timer_setfn(&httpRxTmr, (os_timer_func_t*)params->parserFunc, NULL);
			os_timer_arm(&httpRxTmr, 100, 0);
		}
		else
		{
			debug("httpMsgRxBuf full\n");
			params->parserFunc();
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
	dispSetActiveMemBuf(MainMemBuf);
	dispClearMem(50, 14);

	char timeStr[7];
	int minutes = progress / 60;
	int seconds = progress % 60;
	int len = ets_snprintf(timeStr, sizeof(timeStr), "%d:%02d", minutes, seconds);
	drawStr_Latin(&arial13, 0, 50, timeStr, len);

	minutes = duration / 60;
	seconds = duration % 60;
	len = ets_snprintf(timeStr, sizeof(timeStr), "%d:%02d", minutes, seconds);
	int timeStrWidth = drawStrAlignRight_Latin(&arial13, DISP_WIDTH-1, 50, timeStr, len);

	int barX = timeStrWidth + 7;
	int barMaxWidth = DISP_WIDTH-1 - (barX*2);
	int barXmax = barX+barMaxWidth-1;
	int barY = 59;
	int barHeight = 3;
	int barY2 = barY+barHeight-1;
	drawLine(barX, barY-1, barXmax, barY-1, 1);
	drawLine(barX, barY2+1, barXmax, barY2+1, 1);
	drawLine(barX-1, barY, barX-1, barY2, 1);
	drawLine(barXmax+1, barY, barXmax+1, barY2, 1);

	float progressPercent = ((float)progress) / duration;
	int barWidth = progressPercent * barMaxWidth + 0.5;
	if (barWidth > 0)
	{
		int barX2 = barX+barWidth-1;
		drawRect(barX, barY, barX2, barY2, 1);
	}

	dispUpdate(50, 14);
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


LOCAL void ICACHE_FLASH_ATTR requestTokens(void)
{
    if (spotifyRequestTokens(authConnParams.host, auth_code) == OK)
    {
        setAppState(stateRequestSent);
    }
}

LOCAL void ICACHE_FLASH_ATTR refreshTokens(void)
{
	if (spotifyRefreshTokens(authConnParams.host, config.refresh_token) == OK)
	{
		setAppState(stateRequestSent);
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




LOCAL void ICACHE_FLASH_ATTR parseAuthReply(void)
{
	setAppState(stateReplyReceived);

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

		int expiresIn;
		if (parseTokens(json, jsonLen,
				config.access_token, sizeof(config.access_token),
				config.refresh_token, sizeof(config.refresh_token),
				&expiresIn) != OK)
		{
			debug("parseTokens failed\n");
			return;
		}
		uint ts = sntp_get_current_timestamp();
		config.tokenExpireTs = ts + expiresIn;
		configWrite(&config);

		connectToHost(&apiConnParams);
	}
	
	httpRxMsgCurLen = 0;
}

LOCAL void ICACHE_FLASH_ATTR parseApiReply(void)
{
	setAppState(stateReplyReceived);

	debug("parseApiReply, len %d\n", httpRxMsgCurLen);
	httpMsgRxBuf[httpRxMsgCurLen] = '\0';
	//debug("%s\n", httpMsgRxBuf);

	if (apiConnParams.requestFunc == getCurrentlyPlaying)
	{
		char *json = (char*)os_strstr(httpMsgRxBuf, "{");
		if (!json)
		{
			httpRxMsgCurLen = 0;
			return;
		}
		int jsonLen = httpRxMsgCurLen - (json - httpMsgRxBuf);
		//debug("jsonLen %d\n", jsonLen);

		TrackInfo track;
		os_memset(&track, 0, sizeof(TrackInfo));
		if (parseTrackInfo(json, jsonLen, &track) == OK)
		{
			if (wstrcmp(curTrack.name.str, track.name.str))
			{
				debug("new track\n");
				dispSetActiveMemBuf(TitleMemBuf);
				dispClearMemAll();
				drawStr(&arial13b, 0, 0, track.name.str, track.name.length);

				if (!strListEqual(&curTrack.artists, &track.artists))
				{
					debug("new artist\n");
					dispSetActiveMemBuf(ArtistMemBuf);
					dispClearMemAll();

					StrBuf separator;
					ushort sepStr[] = {',', ' ', '\0'};
					separator.str = sepStr;
					separator.length = NELEMENTS(sepStr)-1;
					strListDraw(&arial13, 0, 0, &track.artists, &separator);
					scrollTitleArtist();
				}
				else
				{
					debug("same artist\n");
					scrollTitle();
				}

				wakeupDisplay();
			}
			else
			{
				debug("same track\n");
				if (!curTrack.isPlaying && track.isPlaying)
				{
					wakeupDisplay();
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
			debug("parseTrack failed\n");
			trackInfoFree(&track);
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




typedef enum{
	NotPressed,
	Button1,
	Button2
}Button;

LOCAL Button adcValToButton(uint16 adcVal)
{
	if (adcVal >= 768) return NotPressed;
	if (adcVal >= 256) return Button1;
	return Button2;
}

LOCAL void ICACHE_FLASH_ATTR buttonsScanTmrCb(void)
{
	static Button prevButtons = NotPressed;
	uint16 adcVal = system_adc_read();
	Button buttons = adcValToButton(adcVal);
	//debug("adcVal %d, buttons %d\n", adcVal, buttons);
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
		dispVerticalSqueezeStart();
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

