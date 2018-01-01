#include <ets_sys.h>
#include "common.h"

uint spiFlashReadDword(const uint *addr)
{
    uint value;
    spi_flash_read((uint)addr, &value, sizeof(uint));
    return value;
}

// multiple must be power of 2
int ICACHE_FLASH_ATTR roundUp(int numToRound, int multiple)
{
	return (numToRound + multiple - 1) & -multiple;
}
