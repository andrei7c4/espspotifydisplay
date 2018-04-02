#include <osapi.h>
#include <os_type.h>
#include "graphics.h"
#include "SSD1322.h"
#include "SH1106.h"
#include "display.h"


void ICACHE_FLASH_ATTR dispUpdate(int row, int height)
{
#if DISP_TYPE == 1322
	SSD1322_cpyMemBuf(MainGfxBuf.buf, MainGfxBuf.memWidth, row, row, height);
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
	int pages = height/8;
	if (height%8)
	{
		pages++;
	}
	int mod = row % 8;
	if (mod)
	{
		pages++;
	}
	SH1106_SSD1306_cpyMemBuf(MainGfxBuf.buf, MainGfxBuf.memWidth, row-mod, row/8, pages);
#endif
}

void ICACHE_FLASH_ATTR dispUpdateFull(void)
{
	dispUpdate(0, DISP_HEIGHT);
}

// combined update is more efficient on SSD1306 and SH1106
void ICACHE_FLASH_ATTR dispUpdateLabels(int title, int artist, int album)
{
	int labels = title | (artist << 1) | (album << 2);
	switch (labels)
	{
	case 1:		// title only
		dispUpdate(TitleLabel.offset, TitleLabel.buf.height);
		break;
	case 2:		// artist only
		dispUpdate(ArtistLabel.offset, ArtistLabel.buf.height);
		break;
	case 3:		// title & artist
		dispUpdate(TitleLabel.offset, ArtistLabel.offset + ArtistLabel.buf.height);
		break;
	case 4:		// album only
		dispUpdate(AlbumLabel.offset, AlbumLabel.buf.height);
		break;
	case 5:		// title & album
		dispUpdate(TitleLabel.offset, TitleLabel.buf.height);
		dispUpdate(AlbumLabel.offset, AlbumLabel.buf.height);
		break;
	case 6:		// artist & album
		dispUpdate(ArtistLabel.offset, AlbumLabel.offset + AlbumLabel.buf.height);
		break;
	case 7:		// title & artist & album
		dispUpdate(TitleLabel.offset, AlbumLabel.offset + AlbumLabel.buf.height);
		break;
	}
}

void ICACHE_FLASH_ATTR dispUpdateProgBar(void)
{
	dispUpdate(PROGBAR_OFFSET, PROGBAR_HEIGHT);
}

void ICACHE_FLASH_ATTR dispClearBlankSpace(int showAlbum)
{
	int blankOffset = showAlbum ? (AlbumLabel.offset + AlbumLabel.buf.height) :
								  	  	 (ArtistLabel.offset + ArtistLabel.buf.height);
	int blankHeight = PROGBAR_OFFSET - blankOffset;
	activeBuf = &MainGfxBuf;
	activeBufClear(blankOffset, blankHeight);
	dispUpdate(blankOffset, blankHeight);
}


DispState displayState = stateOff;
Orientation dispOrient = orient0deg;


LOCAL os_timer_t dimmingTmr;
LOCAL int contrastCurValue = CONTRAST_LEVEL_INIT;
LOCAL int contrastSetPointValue = CONTRAST_LEVEL_INIT;
LOCAL int contrastIncValue = 0;
LOCAL int contrastCtrlTrmInt = 5;

LOCAL int squeezeRow = 0;
LOCAL int squeezeColumn = 0;


LOCAL void ICACHE_FLASH_ATTR horizontalSqueezeTmrCb(void)
{
	if (squeezeColumn <= 127)
	{
		drawPixel(squeezeColumn, 0, 0);
		drawPixel(DISP_WIDTH-1-squeezeColumn, 0, 0);
		SSD1322_cpyMemBuf(activeBuf->buf, activeBuf->memWidth, 0, squeezeRow, 1);
		squeezeColumn++;
	}
	else
	{
		os_timer_disarm(&dimmingTmr);
		SSD1322_setOnOff(stateOff);
		SSD1322_partialDispDis();

		if (contrastCurValue > 0)
		{
			contrastCurValue = 0;	// for smooth undimm
			SSD1322_setContrast(contrastCurValue);
		}

		activeBuf = &MainGfxBuf;
		SSD1322_cpyMemBuf(activeBuf->buf, activeBuf->memWidth, squeezeRow, squeezeRow, 1);	// restore middle row
	}
}

LOCAL void ICACHE_FLASH_ATTR horizontalSqueezeStart(void)
{
	activeBuf = &TempGfxBuf;
	activeBufClearAll();
	squeezeColumn = 30;
	int x;
	for (x = squeezeColumn; x < DISP_WIDTH-squeezeColumn; x++)
	{
		drawPixel(x, 0, 1);
	}
	SSD1322_cpyMemBuf(activeBuf->buf, activeBuf->memWidth, 0, squeezeRow, 1);

	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)horizontalSqueezeTmrCb, NULL);
	os_timer_arm(&dimmingTmr, 5, 1);
}

LOCAL void ICACHE_FLASH_ATTR verticalSqueezeTmrCb(void)
{
	squeezeRow++;
	if (squeezeRow <= 31)
	{
		SSD1322_partialDispEn(squeezeRow, DISP_HEIGHT-1-squeezeRow);
	}
	else
	{
		SSD1322_partialDispEn(squeezeRow, squeezeRow);
		horizontalSqueezeStart();
	}
}

void ICACHE_FLASH_ATTR dispSmoothTurnOff(void)
{
#if DISP_TYPE == 1322
	squeezeRow = 0;
	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)verticalSqueezeTmrCb, NULL);
	os_timer_arm(&dimmingTmr, 10, 1);
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
	// squeezing effects are not implement for SH1106 yet
	// so just turn the display off
	SH1106_SSD1306_setOnOff(stateOff);
#endif
}

LOCAL void ICACHE_FLASH_ATTR constrastCtrlTmrCb(void)
{
	contrastCurValue += contrastIncValue;
#if DISP_TYPE == 1322
	SSD1322_setContrast(contrastCurValue);
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
	SH1106_SSD1306_setContrast(contrastCurValue);
#endif
	if (contrastCurValue != contrastSetPointValue)
	{
		os_timer_arm(&dimmingTmr, contrastCtrlTrmInt, 0);
	}
}

void ICACHE_FLASH_ATTR dispDimmingStart(void)
{
	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)constrastCtrlTmrCb, NULL);
	contrastCurValue = 254;
	contrastSetPointValue = 0;
	contrastIncValue = -2;
	contrastCtrlTrmInt = 15;
	constrastCtrlTmrCb();
	displayState = stateDimmed;
}

void ICACHE_FLASH_ATTR dispUndimmStart(void)
{
	int prevState = displayState;
	switch (displayState)
	{
	case stateOn: return;
		break;
	case stateOff:
#if DISP_TYPE == 1322
		SSD1322_setOnOff(stateOn);
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
		SH1106_SSD1306_setOnOff(stateOn);
#endif
		// fallthrough
	case stateDimmed:
		os_timer_disarm(&dimmingTmr);
		os_timer_setfn(&dimmingTmr, (os_timer_func_t *)constrastCtrlTmrCb, NULL);
		//contrastCurValue = 0;
		contrastSetPointValue = 254;
		contrastIncValue = 2;
		contrastCtrlTrmInt = 5;
		if (prevState == stateOff)
		{
			os_timer_arm(&dimmingTmr, 300, 0);
		}
		else
		{
			constrastCtrlTmrCb();
		}
		displayState = stateOn;
		break;
	}
}


void ICACHE_FLASH_ATTR dispSetOrientation(Orientation orientation)
{
#if DISP_TYPE == 1322
	switch (orientation)
	{
	case orient0deg:
		SSD1322_setRemap(0x14, 0x11);
		break;
	case orient180deg:
		SSD1322_setRemap(0x06, 0x11);
		break;
	default: return;
	}
#elif DISP_TYPE == 1106 || DISP_TYPE == 1306
	switch (orientation)
	{
	case orient0deg:
		SH1106_SSD1306_setSegmentRemap(eNormalDir);
		SH1106_SSD1306_setComOutScanDir(eScanFrom0);
#if DISP_HEIGHT == 32
		SH1106_SSD1306_setDispVoffset(0);
#endif
		break;
	case orient180deg:
		SH1106_SSD1306_setSegmentRemap(eReverseDir);
		SH1106_SSD1306_setComOutScanDir(eScanTo0);
#if DISP_HEIGHT == 32
		SH1106_SSD1306_setDispVoffset(32);
#endif
		break;
	default: return;
	}
#endif
	dispUpdateFull();
	dispOrient = orientation;
}

