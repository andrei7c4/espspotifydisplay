#include <ets_sys.h>
#include "common.h"

uint spiFlashReadDword(const uint *addr)
{
    uint value;
    spi_flash_read((uint)addr, &value, sizeof(uint));
    return value;
}
