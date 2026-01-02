#include "mem_map.h"

extern uint8_t  vflash[FLASH_SIZE];
extern void flash_init(uint8_t* data, uint32_t size);
extern "C" void flash_read(int32_t addr, int32_t* data);
