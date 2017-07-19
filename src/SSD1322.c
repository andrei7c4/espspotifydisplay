#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <user_interface.h>
#include <gpio.h>
#include <mem.h>
#include "drivers/spi.h"
#include "SSD1322.h"
#include "common.h"
#include "graphics.h"


typedef enum{
	eCmd = 0,
	eData = 1
}DataType;

// SSD1322 register offsets
#define REG_GRAYSCALE_TAB_EN        0x00
#define REG_COLUMN_ADDR             0x15
#define REG_WRITE_RAM_CMD           0x5C
#define REG_READ_RAM_CMD            0x5D
#define REG_ROW_ADDR                0x75
#define REG_REMAP_CONFIG            0xA0
#define REG_START_LINE              0xA1
#define REG_DISPLAY_OFFSET          0xA2
#define REG_PIXELS_OFF              0xA4
#define REG_PIXELS_ON               0xA5
#define REG_PIXELS_NORM             0xA6
#define REG_PIXELS_INVERSE          0xA7
#define REG_PART_DISP_EN            0xA8
#define REG_PART_DISP_DIS           0xA9
#define REG_INTERNAL_VDD_CTRL       0xAB
#define REG_DISPLAY_OFF             0xAE
#define REG_DISPLAY_ON              0xAF
#define REG_PHASE_LENGTH            0xB1
#define REG_CLOCK_CONFIG            0xB3
#define REG_GPIO_CONFIG             0xB5
#define REG_SEC_PRECHRG_PERIOD      0xB6
#define REG_GRAYSCALE_TABLE         0xB8
#define REG_GRAYSCALE_TAB_DEFAULT   0xB9
#define REG_PRECHRG_VOLT            0xBB
#define REG_COM_VOLT                0xBE
#define REG_CONTRAST_CURRENT        0xC1
#define REG_CONTRAST_CURR_CTRL      0xC7
#define REG_MUX_RATIO               0xCA
#define REG_CMD_LOCK                0xFD


// Adapted from Espressif example:
// http://bbs.espressif.com/viewtopic.php?f=31&t=1346
void SSD1322_write(uchar low_8bit, uchar high_bit)
{
	uint regvalue;
	uchar bytetemp;

	if (high_bit)
	{
		bytetemp = (low_8bit>>1) | 0x80;
	}
	else
	{
		bytetemp = (low_8bit>>1) & 0x7f;
	}

    // configure transmission variable, 9bit transmission length and first 8 command bit
	regvalue = ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint)bytetemp);

	if(low_8bit&0x01)
	{
		regvalue |= BIT15;        //write the 9th bit
	}
	
	while (READ_PERI_REG(SPI_CMD(HSPI)) & SPI_USR)
	{
		// waiting for spi module available
	}
	WRITE_PERI_REG(SPI_USER2(HSPI), regvalue);	// write command and command length into spi reg
	SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);	// transmission start
}


LOCAL void ICACHE_FLASH_ATTR SSD1322_setRowAddr(uchar addr)
{
    SSD1322_write(REG_ROW_ADDR, eCmd);
	SSD1322_write(addr, eData);
	SSD1322_write(addr+63, eData);
}

LOCAL void ICACHE_FLASH_ATTR SSD1322_setColumnAddr(uchar addr)
{
    SSD1322_write(REG_COLUMN_ADDR, eCmd);
	SSD1322_write(0x1c|addr, eData);
	SSD1322_write(0x5b, eData);
}

void ICACHE_FLASH_ATTR SSD1322_setStartLine(uchar line)
{
	SSD1322_write(REG_START_LINE, eCmd);
	SSD1322_write(line, eData);
}

void ICACHE_FLASH_ATTR SSD1322_setOnOff(DispState state)
{
	SSD1322_write(state == stateOn ? REG_DISPLAY_ON : REG_DISPLAY_OFF, eCmd);
	displayState = state;
}

void ICACHE_FLASH_ATTR SSD1322_setContrast(uchar value)
{
	SSD1322_write(REG_CONTRAST_CURRENT, eCmd);
	SSD1322_write(value, eData);
}

LOCAL void ICACHE_FLASH_ATTR SSD1322_setGrayLevel(uchar level)
{
	SSD1322_write(REG_GRAYSCALE_TABLE, eCmd);
	int i;
	for (i = 0; i < 14; i++)
	{
		//SSD1322_write(i*8, eData);	// default
		SSD1322_write(0, eData);
	}
	SSD1322_write(level, eData);	// we only care about GS15
	SSD1322_write(REG_GRAYSCALE_TAB_EN, eCmd);		// enable gray scale table
}

void ICACHE_FLASH_ATTR SSD1322_partialDispEn(uchar startRow, uchar endRow)
{
	SSD1322_write(REG_PART_DISP_EN, eCmd);
	SSD1322_write(startRow, eData);
	SSD1322_write(endRow, eData);
}

void ICACHE_FLASH_ATTR SSD1322_partialDispDis(void)
{
	SSD1322_write(REG_PART_DISP_DIS, eCmd);
}

void ICACHE_FLASH_ATTR SSD1322_setRemap(uchar paramA, uchar paramB)
{
	SSD1322_write(REG_REMAP_CONFIG, eCmd);
	SSD1322_write(paramA, eData);
	SSD1322_write(paramB, eData);
}

void ICACHE_FLASH_ATTR SSD1322_cpyMemBuf(uchar mem[][DISP_MEMWIDTH], int memRow, uchar dispRow, int height)
{
    int x, y;
	uchar temp,temp1,temp2,temp3,temp4,temp5,temp6,temp7,temp8;
	uchar h11,h12,h13,h14,h15,h16,h17,h18;
	uint d1,d2,d3,d4;

   	SSD1322_setRowAddr(dispRow);
    SSD1322_setColumnAddr(0);
 	SSD1322_write(REG_WRITE_RAM_CMD, eCmd);

	for (y = memRow; y < (memRow+height); y++)
	{
        for (x = 0; x < DISP_MEMWIDTH; x++)
		{
        	// form 8 pixel width block
			temp = mem[y][x];
			temp1=temp&0x80;
			temp2=(temp&0x40)>>3;
			temp3=(temp&0x20)<<2;
			temp4=(temp&0x10)>>1;
			temp5=(temp&0x08)<<4;
			temp6=(temp&0x04)<<1;
			temp7=(temp&0x02)<<6;
			temp8=(temp&0x01)<<3;
			h11=temp1|temp1>>1|temp1>>2|temp1>>3;
			h12=temp2|temp2>>1|temp2>>2|temp2>>3;
			h13=temp3|temp3>>1|temp3>>2|temp3>>3;
			h14=temp4|temp4>>1|temp4>>2|temp4>>3;
			h15=temp5|temp5>>1|temp5>>2|temp5>>3;
			h16=temp6|temp6>>1|temp6>>2|temp6>>3;
			h17=temp7|temp7>>1|temp7>>2|temp7>>3;
			h18=temp8|temp8>>1|temp8>>2|temp8>>3;
			d1=h11|h12;
			d2=h13|h14;
			d3=h15|h16;
			d4=h17|h18;

			// write 8 pixels to display
			SSD1322_write(d1, eData);
			SSD1322_write(d2, eData);
			SSD1322_write(d3, eData);
			SSD1322_write(d4, eData);
		}
	}
}


void ICACHE_FLASH_ATTR SSD1322_init(void)
{
	// reset pin as GPIO
	PIN_FUNC_SELECT(RST_GPIO_MUX, RST_GPIO_FUNC);
	GPIO_OUTPUT_SET(RST_GPIO, 1);
	
	// enable hw-controlled CS
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);

	spi_clock(HSPI, 5, 2);	// 8 MHz

	GPIO_OUTPUT_SET(RST_GPIO, 0);
	os_delay_us(5000);
	GPIO_OUTPUT_SET(RST_GPIO, 1);
	os_delay_us(5000);

	// init SSD1322 registers
	SSD1322_write(REG_CMD_LOCK, eCmd);      
	SSD1322_write(0x12, eData);     // unlock interface
    
	SSD1322_setOnOff(stateOff);
    
	SSD1322_write(REG_CLOCK_CONFIG, eCmd);
	SSD1322_write(0x91, eData);     // front clock div by 2, osc. freq. level 9
    
	SSD1322_write(REG_MUX_RATIO, eCmd);
	SSD1322_write(63, eData);
    
	SSD1322_write(REG_DISPLAY_OFFSET, eCmd); 
	SSD1322_write(0, eData);
    
    SSD1322_setStartLine(0);
    
	SSD1322_setRemap(0x14, 0x11);	// 0 deg orientation
    
	SSD1322_write(REG_INTERNAL_VDD_CTRL, eCmd);
	SSD1322_write(0x01, eData);     // enable internal Vdd regulator
    
	SSD1322_write(0xB4, eCmd);      // Display Enhancement A
	SSD1322_write(0xA0, eData);
	SSD1322_write(0xfd, eData);
    
    SSD1322_setContrast(CONTRAST_LEVEL_INIT);
    
	SSD1322_write(REG_CONTRAST_CURR_CTRL, eCmd);
	SSD1322_write(0x0f, eData);     // full setting
    
	SSD1322_write(REG_PHASE_LENGTH, eCmd);
	SSD1322_write(0xE2, eData);     // phase 1: 5 DCLKs, phase 2: 14 DCLKs
    
	SSD1322_write(0xD1, eCmd);      // Display Enhancement B
	SSD1322_write(0x82, eData); 
	SSD1322_write(0x20, eData); 
    
	SSD1322_write(REG_PRECHRG_VOLT, eCmd);
	SSD1322_write(0x1F, eData);     // 0.6 * Vcc
    
	SSD1322_write(REG_SEC_PRECHRG_PERIOD, eCmd);
	SSD1322_write(0x08, eData);     // 8 DCLKs
    
	SSD1322_write(REG_COM_VOLT, eCmd);
	SSD1322_write(0x07, eData);     // 0.86 * Vcc
    
	SSD1322_write(REG_PIXELS_NORM, eCmd);
    
	SSD1322_setGrayLevel(180);      // set GS15 to max
	
	//SSD1322_setOnOff(stateOn);
	//SSD1322_write(REG_PIXELS_ON, eCmd); // full on
}
