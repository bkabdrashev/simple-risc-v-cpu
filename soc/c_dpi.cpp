#include "svdpi.h"
#include "stdio.h"
#include "assert.h"

#include "mem_map.h"

uint8_t vflash[FLASH_SIZE];

extern "C" void flash_read(int32_t addr, int32_t* data) {
 *data = 
      vflash[addr + 3] << 24 | vflash[addr + 2] << 16 |
      vflash[addr + 1] <<  8 | vflash[addr + 0] <<  0 ;
}

void flash_init(uint8_t* data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    vflash[i] = data[i];
  }
  printf("[INFO] v flash written: %u bytes\n", size);
}
