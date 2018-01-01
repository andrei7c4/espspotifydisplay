#include <osapi.h>
#include <os_type.h>
#include "graphics.h"
#include "SSD1322.h"
#include "display.h"


extern void SSD1322_cpyMemBuf(uchar *mem, int memWidth, int memRow, uchar dispRow, int height);

void ICACHE_FLASH_ATTR dispUpdate(int row, int height)
{
	SSD1322_cpyMemBuf(MainGfxBuf.buf, MainGfxBuf.memWidth, row, row, height);
}

void ICACHE_FLASH_ATTR dispUpdateFull(void)
{
	dispUpdate(0, DISP_HEIGHT);
}

void ICACHE_FLASH_ATTR dispUpdateProgBar(void)
{
	dispUpdate(BLANK_SPACE_OFFSET, BLANK_SPACE_HEIGHT+PROGBAR_HEIGHT);
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

