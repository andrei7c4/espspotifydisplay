#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <user_interface.h>
#include <gpio.h>
#include <mem.h>
#include "drivers/i2c_bitbang.h"
#include "SH1106.h"
#include "common.h"

// Common driver for SH1106 and SSD1306 controllers


#define SH1106_ADDR		0x78

LOCAL int writeCmdByte(uchar data)
{
	if (!i2c_write_byte(TRUE, FALSE, SH1106_ADDR) &&
	   (!i2c_write_byte(FALSE, FALSE, 0x00)) &&
	   (!i2c_write_byte(FALSE, TRUE, data)))
	{
		return OK;
	}
	i2c_stop_cond();
	return ERROR;
}

LOCAL int writeCmdWord(uchar data1, uchar data2)
{
	if (!i2c_write_byte(TRUE, FALSE, SH1106_ADDR) &&
	   (!i2c_write_byte(FALSE, FALSE, 0x00)) &&
	   (!i2c_write_byte(FALSE, FALSE, data1)) &&
	   (!i2c_write_byte(FALSE, TRUE, data2)))
	{
		return OK;
	}
	i2c_stop_cond();
	return ERROR;
}

LOCAL int writeData(uchar *data, int size)
{
	if (i2c_write_byte(TRUE, FALSE, SH1106_ADDR) ||
	   (i2c_write_byte(FALSE, FALSE, 0x40)))
	{
		i2c_stop_cond();
		return ERROR;
	}

	while (size > 0)
	{
		if (i2c_write_byte(FALSE, FALSE, *data))
		{
			i2c_stop_cond();
			return ERROR;
		}
		data++;
		size--;
	}
	i2c_stop_cond();
	return OK;
}

LOCAL int ICACHE_FLASH_ATTR fillData(uchar data, int size)
{
	if (i2c_write_byte(TRUE, FALSE, SH1106_ADDR) ||
	   (i2c_write_byte(FALSE, FALSE, 0x40)))
	{
		i2c_stop_cond();
		return ERROR;
	}

	while (size > 0)
	{
		if (i2c_write_byte(FALSE, FALSE, data))
		{
			i2c_stop_cond();
			return ERROR;
		}
		size--;
	}
	i2c_stop_cond();
	return OK;
}


/// SH1106 and SSD1306 commands

#define COLUMN_ADDR_LO		0x00
#define COLUMN_ADDR_HI		0x10
#define START_LINE_ADDR		0x40
#define CONTRAST_VAL		0x81
#define ALLPIXELS_ON		0xA4
#define DISP_REVERSE		0xA6
#define MULTIPLEX_RATIO		0xA8
#define DCDC_CTRL			0xAD	// SH1106 only
#define DCDC_ONOFF			0x8A	// SH1106 only
#define PAGE_ADDR			0xB0
#define DISP_VOFFSET		0xD3
#define INTOSC_FREQ			0xD5
#define CHARGE_PERIODS		0xD9
#define COMPADS_CONF		0xDA
#define VCOM_DESEL			0xDB

#if DISP_TYPE == 1106
#define COLUMN_OFFSET		2		// might be display module specific
#else
#define COLUMN_OFFSET		0
#endif
LOCAL int ICACHE_FLASH_ATTR setColumnAddr(uchar addr)	// cmd 1 & 2
{
	addr += COLUMN_OFFSET;
	if (writeCmdByte(COLUMN_ADDR_LO | (addr & 0x0F)) == OK &&
	   (writeCmdByte(COLUMN_ADDR_HI | ((addr>>4) & 0x0F)) == OK))
	{
		return OK;
	}
	return ERROR;
}

// cmd 3 (SH1106 only)
typedef enum{
	eVolt7_4 = 0x30,
	eVolt8_0 = 0x31,
	eVolt8_4 = 0x32,
	eVolt9_0 = 0x33,
}ChargePumpVolt;
LOCAL int ICACHE_FLASH_ATTR SH1106_setChargePumpVolt(ChargePumpVolt volt)
{
	return writeCmdByte(volt);
}

LOCAL int ICACHE_FLASH_ATTR setStartLine(uchar line)		// cmd 4
{
	return writeCmdByte(START_LINE_ADDR | (line & 0x3F));
}

int ICACHE_FLASH_ATTR SH1106_SSD1306_setContrast(uchar value)		// cmd 5
{
	return writeCmdWord(CONTRAST_VAL, value);
}

int ICACHE_FLASH_ATTR SH1106_SSD1306_setSegmentRemap(SegmentRemap remap)		// cmd 6
{
	return writeCmdByte(remap);
}

// cmd 7
typedef enum{
	eOff = 0, eOn = 1
}OnOffState;
LOCAL int ICACHE_FLASH_ATTR setAllPixelsOn(OnOffState state)
{
	return writeCmdByte(ALLPIXELS_ON | state);
}

LOCAL int ICACHE_FLASH_ATTR setDispReverse(OnOffState state)		// cmd 8
{
	return writeCmdByte(DISP_REVERSE | state);
}

LOCAL int ICACHE_FLASH_ATTR setMultiplexRatio(uchar value)		// cmd 9
{
	return writeCmdWord(MULTIPLEX_RATIO, value & 0x3F);
}

LOCAL int ICACHE_FLASH_ATTR setDcDcOnOff(OnOffState state)		// cmd 10
{
	return writeCmdWord(DCDC_CTRL, DCDC_ONOFF | state);
}

int ICACHE_FLASH_ATTR SH1106_SSD1306_setOnOff(DispState state)		// cmd 11
{
	displayState = state;
	return writeCmdByte(state == stateOn ? 0xAF : 0xAE);
}

LOCAL int ICACHE_FLASH_ATTR setPageAddr(uchar addr)		// cmd 12
{
	return writeCmdByte(PAGE_ADDR | (addr & 0x07));
}

int ICACHE_FLASH_ATTR SH1106_SSD1306_setComOutScanDir(ComOutScanDir scanDir)		// cmd 13
{
	return writeCmdByte(scanDir);
}

int ICACHE_FLASH_ATTR SH1106_SSD1306_setDispVoffset(uchar offset)		// cmd 14
{
	return writeCmdWord(DISP_VOFFSET, offset & 0x3F);
}

LOCAL int ICACHE_FLASH_ATTR setOscFreq(uchar frequency, uchar divRatio)		// cmd 15
{
	return writeCmdWord(INTOSC_FREQ, ((frequency & 0x0F)<<4) | (divRatio & 0x0F));
}

LOCAL int ICACHE_FLASH_ATTR setChargePeriods(uchar discharge, uchar precharge)	// cmd 16
{
	return writeCmdWord(CHARGE_PERIODS, ((discharge & 0x0F)<<4) | (precharge & 0x0F));
}

typedef enum{
	eSequential = 0x02,
	eAlternative = 0x12
}ComPadsConf;
LOCAL int setComPadsConf(ComPadsConf conf)	// cmd 17
{
	return writeCmdWord(COMPADS_CONF, conf);
}

LOCAL int ICACHE_FLASH_ATTR setVcomDesel(uchar level)	// cmd 18
{
	return writeCmdWord(VCOM_DESEL, level);
}


/// GDRAM update functions

// in: horizontally packed (MSB left) pixel block
// out: vertically packed (LSB up) pixel block
LOCAL void transform64x64block(uchar *in, uchar *out)
{
	int row, col;
	uchar maskIn, maskOut;
	uchar temp;
	for (col = 0, maskIn = 0x80; col < 8; col++, maskIn >>= 1)
	{
		temp = 0;
		for (row = 0, maskOut = 1; row < 8; row++, maskOut <<= 1)
		{
			if (in[row] & maskIn)
			{
				temp |= maskOut;
			}
		}
		out[col] = temp;
	}
}

void SH1106_SSD1306_cpyMemBuf(uchar *mem, int memWidth, int memRow, uchar dispPage, int pages)
{
	int x, y, y2;
	uchar tempH[8];
	uchar tempV[8];

	for (y = memRow; pages > 0 && dispPage < (DISP_HEIGHT/8); pages--, dispPage++, y += 8)
	{
		setColumnAddr(0);
		setPageAddr(dispPage);
        for (x = 0; x < DISP_MEMWIDTH; x++)
		{
        	// form 64x64 pixel block (horizontally packed)
        	for (y2 = 0; y2 < 8; y2++)
    		{
    			tempH[y2] = *(mem + (y+y2)*memWidth + x);
    		}
    		transform64x64block(tempH, tempV);
    		writeData(tempV, sizeof(tempV));
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR fillMem(uchar data)
{
	uchar page;
	for (page = 0; page < (DISP_HEIGHT/8); page++)
	{
		setColumnAddr(0);
		setPageAddr(page);
		if (fillData(data, DISP_WIDTH) != OK)
		{
			break;
		}
	}
}


/// SH1106 initialization

LOCAL os_timer_t initTmr;

LOCAL int nonblockInit;
LOCAL initDoneFunc initDoneCb;

LOCAL void ICACHE_FLASH_ATTR delayMs(int ms)
{
	while (ms--)
	{
		os_delay_us(1000);
		system_soft_wdt_feed();
	}
}

typedef int (*initFunc)(void);
int ICACHE_FLASH_ATTR callWithDelay(int delay, initFunc func)
{
	if (nonblockInit)
	{
		os_timer_disarm(&initTmr);
		os_timer_setfn(&initTmr, (os_timer_func_t*)func, NULL);
		os_timer_arm(&initTmr, delay, 0);
		return OK;
	}
	else
	{
		delayMs(delay);
		return func();
	}
}

LOCAL int SH1106_initStage2(void);
LOCAL int SH1106_initStage3(void);
int ICACHE_FLASH_ATTR SH1106_init(int nonblock, initDoneFunc callback, Orientation orientation, int setDispOn)
{
	nonblockInit = nonblock;
	initDoneCb = callback;

	if (setDcDcOnOff(eOff) == OK &&
		SH1106_SSD1306_setOnOff(stateOff) == OK &&

		SH1106_SSD1306_setSegmentRemap(orientation == orient0deg ? eNormalDir : eReverseDir) == OK &&
		SH1106_SSD1306_setComOutScanDir(orientation == orient0deg ? eScanFrom0 : eScanTo0) == OK &&
		setComPadsConf(eAlternative) == OK &&
		setMultiplexRatio(0x3F) == OK &&
		setOscFreq(5, 0) == OK &&

		setVcomDesel(0x35) == OK &&
		SH1106_SSD1306_setContrast(CONTRAST_LEVEL_INIT) == OK &&
		SH1106_setChargePumpVolt(eVolt7_4) == OK)
	{
		if (setDcDcOnOff(eOn) == OK)
		{
			if (setDispOn)
			{
				// call stage2 after 100 ms
				return callWithDelay(100, SH1106_initStage2);
			}
			else
			{
				// call stage3 after 150 ms
				return callWithDelay(150, SH1106_initStage3);
			}
		}
	}

	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR SH1106_initStage2(void)
{
	fillMem(0x00);
	if (SH1106_SSD1306_setOnOff(stateOn) == OK)
	{
		// call stage3 after 150 ms
		return callWithDelay(150, SH1106_initStage3);
	}

	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR SH1106_initStage3(void)
{
	if (setStartLine(0) == OK &&
		setPageAddr(0) == OK &&
		setColumnAddr(0) == OK)
	{
		if (initDoneCb)
		{
			initDoneCb();
		}
		return OK;
	}
	return ERROR;
}


/// SSD1306 initialization

LOCAL int ICACHE_FLASH_ATTR SSD1306_setChargePumpOnOff(OnOffState state)
{
	return writeCmdWord(0x8D, 0x10 | (state << 2));
}

LOCAL int SSD1306_initStage2(void);
int ICACHE_FLASH_ATTR SSD1306_init(int nonblock, initDoneFunc callback, Orientation orientation, int setDispOn)
{
	nonblockInit = nonblock;
	initDoneCb = callback;

	if (SSD1306_setChargePumpOnOff(eOff) == OK &&
		SH1106_SSD1306_setOnOff(stateOff) == OK &&

		setMultiplexRatio(0x3F) == OK &&
		setStartLine(0) == OK &&
		SH1106_SSD1306_setDispVoffset(orientation == orient0deg ? 0 : DISP_HEIGHT) == OK &&
		SH1106_SSD1306_setSegmentRemap(orientation == orient0deg ? eNormalDir : eReverseDir) == OK &&
		SH1106_SSD1306_setComOutScanDir(orientation == orient0deg ? eScanFrom0 : eScanTo0) == OK &&
		setComPadsConf(eSequential) == OK &&
		SH1106_SSD1306_setContrast(CONTRAST_LEVEL_INIT) == OK &&
		setAllPixelsOn(eOff) == OK &&
		setDispReverse(eOff) == OK &&
		setOscFreq(8, 0) == OK &&
		SSD1306_setChargePumpOnOff(eOn) == OK)
	{
		if (setDispOn)
		{
			// call stage2 after 100 ms
			return callWithDelay(100, SSD1306_initStage2);
		}
		else
		{
			if (initDoneCb)
			{
				callWithDelay(100, initDoneCb);
			}
			return OK;
		}
	}

	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR SSD1306_initStage2(void)
{
	fillMem(0x00);
	if (SH1106_SSD1306_setOnOff(stateOn) == OK)
	{
		if (initDoneCb)
		{
			initDoneCb();
		}
		return OK;
	}
	return ERROR;
}
