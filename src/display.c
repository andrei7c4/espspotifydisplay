#include <osapi.h>
#include <os_type.h>
#include "graphics.h"
#include "SSD1322.h"
#include "display.h"


#define TITLE_SCROLL_INTERVAL		20

extern void SSD1322_cpyMemBuf(uchar mem[][DISP_MEMWIDTH], int memRow, uchar dispRow, int height);

void ICACHE_FLASH_ATTR dispUpdate(int row, int height)
{
	SSD1322_cpyMemBuf(mem1, row, row, height);
}

void ICACHE_FLASH_ATTR dispUpdateFull(void)
{
	dispUpdate(0, DISP_HEIGHT);
}

void ICACHE_FLASH_ATTR dispUpdateTitle(void)
{
	dispUpdate(0, TITLE_HEIGHT);
}

void ICACHE_FLASH_ATTR dispUpdateTitleArtist(void)
{
	dispUpdate(0, ARTIST_OFFSET+ARTIST_HEIGHT);
}

void ICACHE_FLASH_ATTR dispUpdateProgBar(void)
{
	dispUpdate(BLANK_SPACE_OFFSET, BLANK_SPACE_HEIGHT+PROGBAR_HEIGHT);
}


DispState displayState = stateOff;
Orientation dispOrient = orient0deg;


os_timer_t scrollTmr;
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
		drawPixelNormal(squeezeColumn, 0, 0);
		drawPixelNormal(255-squeezeColumn, 0, 0);
		SSD1322_cpyMemBuf(pMem, 0, squeezeRow, 1);
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

		dispSetActiveMemBuf(MainMemBuf);
		SSD1322_cpyMemBuf(pMem, squeezeRow, squeezeRow, 1);	// restore middle row
	}
}



LOCAL void ICACHE_FLASH_ATTR titleScrollTmrCb(void)
{
	if (titleScrollStep())
	{
		os_timer_disarm(&scrollTmr);
	}
	dispUpdateTitle();
}

void ICACHE_FLASH_ATTR scrollTitle(void)
{
	titleScrollInit();
	os_timer_disarm(&scrollTmr);
	os_timer_setfn(&scrollTmr, (os_timer_func_t *)titleScrollTmrCb, NULL);
	os_timer_arm(&scrollTmr, TITLE_SCROLL_INTERVAL, 1);
}


LOCAL void ICACHE_FLASH_ATTR titleArtistScrollTmrCb(void)
{
	if (titleScrollStep() & artistScrollStep())
	{
		os_timer_disarm(&scrollTmr);
	}
	dispUpdateTitleArtist();
}

void ICACHE_FLASH_ATTR scrollTitleArtist(void)
{
	titleScrollInit();
	artistScrollInit();
	os_timer_disarm(&scrollTmr);
	os_timer_setfn(&scrollTmr, (os_timer_func_t *)titleArtistScrollTmrCb, NULL);
	os_timer_arm(&scrollTmr, TITLE_SCROLL_INTERVAL, 1);
}




LOCAL void ICACHE_FLASH_ATTR horizontalSqueezeStart(void)
{
	dispSetActiveMemBuf(TempMemBuf);
	dispMemFill(0, 0, 1);
	squeezeColumn = 30;
	int x;
	for (x = squeezeColumn; x < DISP_WIDTH-squeezeColumn; x++)
	{
		drawPixelNormal(x, 0, 1);
	}
	SSD1322_cpyMemBuf(pMem, 0, squeezeRow, 1);

	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)horizontalSqueezeTmrCb, NULL);
	os_timer_arm(&dimmingTmr, 5, 1);
}

LOCAL void ICACHE_FLASH_ATTR verticalSqueezeTmrCb(void)
{
	squeezeRow++;
	if (squeezeRow <= 31)
	{
		SSD1322_partialDispEn(squeezeRow, 63-squeezeRow);
	}
	else
	{
		SSD1322_partialDispEn(squeezeRow, squeezeRow);
		horizontalSqueezeStart();
	}
}

void ICACHE_FLASH_ATTR dispVerticalSqueezeStart(void)
{
	squeezeRow = 0;
	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)verticalSqueezeTmrCb, NULL);
	os_timer_arm(&dimmingTmr, 10, 1);
}

LOCAL void ICACHE_FLASH_ATTR SSD1322_constrastCtrlTmrCb(void)
{
	contrastCurValue += contrastIncValue;
	SSD1322_setContrast(contrastCurValue);
	if (contrastCurValue != contrastSetPointValue)
	{
		os_timer_arm(&dimmingTmr, contrastCtrlTrmInt, 0);
	}
}

void ICACHE_FLASH_ATTR dispDimmingStart(void)
{
	os_timer_disarm(&dimmingTmr);
	os_timer_setfn(&dimmingTmr, (os_timer_func_t *)SSD1322_constrastCtrlTmrCb, NULL);
	contrastCurValue = 254;
	contrastSetPointValue = 0;
	contrastIncValue = -2;
	contrastCtrlTrmInt = 15;
	SSD1322_constrastCtrlTmrCb();
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
		//SSD1322_setGrayLevel(180);
		SSD1322_setOnOff(stateOn);
		// fallthrough
	case stateDimmed:
		os_timer_disarm(&dimmingTmr);
		os_timer_setfn(&dimmingTmr, (os_timer_func_t *)SSD1322_constrastCtrlTmrCb, NULL);
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
			SSD1322_constrastCtrlTmrCb();
		}
		displayState = stateOn;
		break;
	}
}


void ICACHE_FLASH_ATTR dispSetOrientation(Orientation orientation)
{
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
	dispUpdateFull();
	dispOrient = orientation;
}

