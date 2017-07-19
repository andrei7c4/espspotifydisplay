#include <osapi.h>
#include <os_type.h>
#include "graphics.h"
#include "SSD1322.h"
#include "display.h"



extern void SSD1322_cpyMemBuf(uchar mem[][DISP_MEMWIDTH], int memRow, uchar dispRow, int height);
void ICACHE_FLASH_ATTR dispUpdate(DispPage page)
{
	SSD1322_cpyMemBuf(mem, 0, page*DISP_HEIGHT, DISP_HEIGHT);
}


DispState displayState = stateOff;
Orientation dispOrient = orient0deg;
int dispScrollCurLine = 0;


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
		SSD1322_cpyMemBuf(mem2, 0, dispScrollCurLine+squeezeRow, 1);
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

		SSD1322_cpyMemBuf(mem, squeezeRow, dispScrollCurLine+squeezeRow, 1);	// restore middle row
		dispSetActiveMemBuf(MainMemBuf);
	}
}

LOCAL void ICACHE_FLASH_ATTR horizontalSqueezeStart(void)
{
	dispSetActiveMemBuf(SecondaryMemBuf);
	dispFillMem(0, 1);
	squeezeColumn = 30;
	int x;
	for (x = squeezeColumn; x < DISP_WIDTH-squeezeColumn; x++)
	{
		drawPixelNormal(x, 0, 1);
	}
	SSD1322_cpyMemBuf(mem2, 0, dispScrollCurLine+squeezeRow, 1);

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


extern void displayScrollDone(void);
// exponential easing in (approx. 0.5s): pow(2, 10*(((curLine-1)/62)-1))*40+1
LOCAL const uchar scrollIntervals[63] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,4,4,4,5,5,6,6,7,8,8,9,10,11,13,14,16,17,19,21,24,27,30,33,37,41};
LOCAL void ICACHE_FLASH_ATTR displayScrollTmrCb(void)
{
	dispScrollCurLine++;
	dispScrollCurLine &= 0x7F;
	SSD1322_setStartLine(dispScrollCurLine);
	if (dispScrollCurLine == 0 || dispScrollCurLine == 64)
	{
		displayScrollDone();
	}
	else
	{
		int line = dispScrollCurLine >= 65 ? dispScrollCurLine-65 : dispScrollCurLine-1;
		os_timer_arm(&scrollTmr, scrollIntervals[line], 0);
	}
}

void ICACHE_FLASH_ATTR scrollDisplay(void)
{
	os_timer_disarm(&scrollTmr);

	if (dispScrollCurLine == 0 || dispScrollCurLine == 64)
	{
		dispUpdate(dispScrollCurLine == 0 ? Page1 : Page0);
	}
	else	// interrupted scroll
	{
		dispUpdate(dispScrollCurLine < 64 ? Page1 : Page0);
	}

	os_timer_disarm(&scrollTmr);
	os_timer_setfn(&scrollTmr, (os_timer_func_t *)displayScrollTmrCb, NULL);
	os_timer_arm(&scrollTmr, 1, 0);
}


void ICACHE_FLASH_ATTR dispSetOrientation(Orientation orientation)
{
	if (dispScrollCurLine != 0 && dispScrollCurLine != 64)
	{
		return;		// do not rotate while scrolling
	}
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
	dispUpdate(dispScrollCurLine == 0 ? Page0 : Page1);
	dispOrient = orientation;
}

