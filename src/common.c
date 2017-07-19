#include <ets_sys.h>
#include "common.h"

int ICACHE_FLASH_ATTR clampInt(int value, int min, int max)
{
    return value < min ? min : (value > max ? max : value);
}


uint spiFlashReadDword(const uint *addr)
{
    uint value;
    spi_flash_read((uint)addr, &value, sizeof(uint));
    return value;
}
