#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <user_interface.h>
#include <gpio.h>
#include <mem.h>
#include "drivers/spi.h"
#include "mpu6500.h"


// MPU6500 register offsets
#define REG_ACCEL_XOUT_H	0x3B
#define REG_USER_CTRL		0x6A
#define REG_PWR_MGMT_1		0x6B
#define REG_WHO_AM_I		0x75

// MPU6500 CS pin
#define MPU_CS_GPIO		4
#define MPU_CS_MUX		PERIPHS_IO_MUX_GPIO4_U
#define MPU_CS_FUNC		FUNC_GPIO4


LOCAL uint8 ICACHE_FLASH_ATTR mpu6500_read8(uint8 offset)
{
	uint8 value;
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 0);
	value = spi_transaction(HSPI, 0, 0, 8, 0x80 | offset, 0, 0, 8, 0);
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 1);
	return value;
}

LOCAL uint16 ICACHE_FLASH_ATTR mpu6500_read16(uint8 offset)
{
	uint16 value;
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 0);
	value = spi_transaction(HSPI, 0, 0, 8, 0x80 | offset, 0, 0, 16, 0);
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 1);
	return value;
}

LOCAL void ICACHE_FLASH_ATTR mpu6500_write(uint8 offset, uint8 value)
{
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 0);
	spi_transaction(HSPI, 0, 0, 8, offset, 8, value, 0, 0);
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 1);
}

int ICACHE_FLASH_ATTR mpu6500_init(void)
{
	int rv = ERROR;
	GPIO_OUTPUT_SET(MPU_CS_GPIO, 1);
	PIN_FUNC_SELECT(MPU_CS_MUX, MPU_CS_FUNC);

	// backup current spi config
	spi_conf_regs regs;
	spi_get_conf_regs(&regs);

	if (mpu6500_read8(REG_WHO_AM_I) == 0x70)
	{
		// mpu6500 found
		mpu6500_write(REG_PWR_MGMT_1, 0x81);	// reset device
		mpu6500_write(REG_USER_CTRL, 0x10);		// disable I2C interface
		rv = OK;
	}

	// restore spi config
	spi_set_conf_regs(&regs);

	return rv;
}

sint16 ICACHE_FLASH_ATTR accelReadX(void)
{
	// backup current spi config
	spi_conf_regs regs;
	spi_get_conf_regs(&regs);

	// disable hw-controlled CS for display
	spi_hw_cs_disable();

	sint16 x = mpu6500_read16(REG_ACCEL_XOUT_H);

	// re-enable CS for display
	spi_hw_cs_enable();

	// restore spi config
	spi_set_conf_regs(&regs);

	return x;
}
