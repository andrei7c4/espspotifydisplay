#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <user_interface.h>
#include <gpio.h>
#include <mem.h>
#include "drivers/i2c_bitbang.h"
#include "SH1106.h"
#include "common.h"


#define SH1106_ADDR		0x78

LOCAL int SH1106_writeCmdByte(uchar data)
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

LOCAL int SH1106_writeCmdWord(uchar data1, uchar data2)
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

LOCAL int SH1106_writeData(uchar *data, int size)
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

LOCAL int ICACHE_FLASH_ATTR SH1106_fillData(uchar data, int size)
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


/// SH1106 commands

#define COLUMN_ADDR_LO		0x00
#define COLUMN_ADDR_HI		0x10
#define START_LINE_ADDR		0x40
#define CONTRAST_VAL		0x81
#define ALLPIXELS_ON		0xA4
#define DISP_REVERSE		0xA6
#define MULTIPLEX_RATIO		0xA8
#define DCDC_CTRL			0xAD
#define DCDC_ONOFF			0x8A
#define PAGE_ADDR			0xB0
#define DISP_VOFFSET		0xD3
#define INTOSC_FREQ			0xD5
#define CHARGE_PERIODS		0xD9
#define COMPADS_CONF		0xDA
#define VCOM_DESEL			0xDB

#define COLUMN_OFFSET		2		// might be display module specific
int ICACHE_FLASH_ATTR SH1106_setColumnAddr(uchar addr)	// cmd 1 & 2
{
	addr += COLUMN_OFFSET;
	if (SH1106_writeCmdByte(COLUMN_ADDR_LO | (addr & 0x0F)) == OK &&
	   (SH1106_writeCmdByte(COLUMN_ADDR_HI | ((addr>>4) & 0x0F)) == OK))
	{
		return OK;
	}
	return ERROR;
}

// cmd 3
typedef enum{
	eVolt7_4 = 0x30,
	eVolt8_0 = 0x31,
	eVolt8_4 = 0x32,
	eVolt9_0 = 0x33,
}ChargePumpVolt;
LOCAL int ICACHE_FLASH_ATTR SH1106_setChargePumpVolt(ChargePumpVolt volt)
{
	return SH1106_writeCmdByte(volt);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setStartLine(uchar line)		// cmd 4
{
	return SH1106_writeCmdByte(START_LINE_ADDR | (line & 0x3F));
}

int ICACHE_FLASH_ATTR SH1106_setContrast(uchar value)		// cmd 5
{
	return SH1106_writeCmdWord(CONTRAST_VAL, value);
}

int ICACHE_FLASH_ATTR SH1106_setSegmentRemap(SegmentRemap remap)		// cmd 6
{
	return SH1106_writeCmdByte(remap);
}

// cmd 7
typedef enum{
	eOff = 0, eOn = 1
}OnOffState;
LOCAL int ICACHE_FLASH_ATTR SH1106_setAllPixelsOn(OnOffState state)
{
	return SH1106_writeCmdByte(ALLPIXELS_ON | state);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setDispReverse(OnOffState state)		// cmd 8
{
	return SH1106_writeCmdByte(DISP_REVERSE | state);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setMultiplexRatio(uchar value)		// cmd 9
{
	return SH1106_writeCmdWord(MULTIPLEX_RATIO, value & 0x3F);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setDcDcOnOff(OnOffState state)		// cmd 10
{
	return SH1106_writeCmdWord(DCDC_CTRL, DCDC_ONOFF | state);
}

int ICACHE_FLASH_ATTR SH1106_setOnOff(DispState state)		// cmd 11
{
	displayState = state;
	return SH1106_writeCmdByte(state == stateOn ? 0xAF : 0xAE);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setPageAddr(uchar addr)		// cmd 12
{
	return SH1106_writeCmdByte(PAGE_ADDR | (addr & 0x07));
}

int ICACHE_FLASH_ATTR SH1106_setComOutScanDir(ComOutScanDir scanDir)		// cmd 13
{
	return SH1106_writeCmdByte(scanDir);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setDispVoffset(uchar offset)		// cmd 14
{
	return SH1106_writeCmdWord(DISP_VOFFSET, offset & 0x3F);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setOscFreq(uchar frequency, uchar divRatio)		// cmd 15
{
	return SH1106_writeCmdWord(INTOSC_FREQ, ((frequency & 0x0F)<<4) | (divRatio & 0x0F));
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setChargePeriods(uchar discharge, uchar precharge)	// cmd 16
{
	return SH1106_writeCmdWord(CHARGE_PERIODS, ((discharge & 0x0F)<<4) | (precharge & 0x0F));
}

typedef enum{
	eSequential = 0x02,
	eAlternative = 0x12
}ComPadsConf;
LOCAL int SH1106_setComPadsConf(ComPadsConf conf)	// cmd 17
{
	return SH1106_writeCmdWord(COMPADS_CONF, conf);
}

LOCAL int ICACHE_FLASH_ATTR SH1106_setVcomDesel(uchar level)	// cmd 18
{
	return SH1106_writeCmdWord(VCOM_DESEL, level);
}


/// SH1106 GDRAM update

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

void SH1106_cpyMemBuf(uchar *mem, int memWidth, int memRow, uchar dispPage, int pages)
{
	int x, y, y2;
	uchar tempH[8];
	uchar tempV[8];

	for (y = memRow; pages > 0; pages--, dispPage++, y += 8)
	{
		SH1106_setColumnAddr(0);
		SH1106_setPageAddr(dispPage);
        for (x = 0; x < DISP_MEMWIDTH; x++)
		{
        	// form 64x64 pixel block (horizontally packed)
        	for (y2 = 0; y2 < 8; y2++)
    		{
    			tempH[y2] = *(mem + (y+y2)*memWidth + x);
    		}
    		transform64x64block(tempH, tempV);
    		SH1106_writeData(tempV, sizeof(tempV));
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR SH1106_fillMem(uchar data)
{
	uchar page;
	for (page = 0; page < 8; page++)
	{
		SH1106_setColumnAddr(0);
		SH1106_setPageAddr(page);
		if (SH1106_fillData(data, DISP_WIDTH) != OK)
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
int ICACHE_FLASH_ATTR SH1106_init(int nonblock, initDoneFunc callback, int setDispOn)
{
	nonblockInit = nonblock;
	initDoneCb = callback;

	if (SH1106_setDcDcOnOff(eOff) == OK &&
		SH1106_setOnOff(stateOff) == OK &&

		SH1106_setSegmentRemap(eNormalDir) == OK &&
		SH1106_setComPadsConf(eAlternative) == OK &&
		SH1106_setComOutScanDir(eScanFrom0) == OK &&
		SH1106_setMultiplexRatio(0x3F) == OK &&
		SH1106_setOscFreq(5, 0) == OK &&

		SH1106_setVcomDesel(0x35) == OK &&
		SH1106_setContrast(CONTRAST_LEVEL_INIT) == OK &&
		SH1106_setChargePumpVolt(eVolt7_4) == OK)
	{
		if (setDispOn)
		{
			SH1106_fillMem(0x00);
		}

		if (SH1106_setDcDcOnOff(eOn) == OK)
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
	if (SH1106_setOnOff(stateOn) == OK)
	{
		// call stage3 after 150 ms
		return callWithDelay(150, SH1106_initStage3);
	}

	return ERROR;
}

LOCAL int ICACHE_FLASH_ATTR SH1106_initStage3(void)
{
	if (SH1106_setStartLine(0) == OK &&
		SH1106_setPageAddr(0) == OK &&
		SH1106_setColumnAddr(0) == OK)
	{
		if (initDoneCb)
		{
			initDoneCb();
		}
		return OK;
	}
	return ERROR;
}
