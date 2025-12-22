#include "svdpi.h"
#include "memory.h"
#include "stdio.h"

#define MEM_START (0x00000000)
#define MEM_END  (128 * 1024 * 1024)
#define MEM_SIZE (MEM_END - MEM_START)
#define VGA_START (0x20000000)
#define VGA_END  (0x20040000)
#define VGA_SIZE (VGA_END - VGA_START)

static uint8_t memory[MEM_SIZE];
static uint8_t vga[VGA_SIZE];

extern "C" void mem_read(uint32_t address, uint32_t* result) {
  if (address >= VGA_START && address < VGA_END) {
    address -= VGA_START;
    *result = 
        vga[address + 3] << 24 | vga[address + 2] << 16 |
        vga[address + 1] <<  8 | vga[address + 0] <<  0 ;
  }
  else if (address >= MEM_START && address < MEM_END) {
    *result = 
      memory[address + 3] << 24 | memory[address + 2] << 16 |
      memory[address + 1] <<  8 | memory[address + 0] <<  0 ;

  }
  else {
    printf("DUT WARNING: mem_read memory is not mapped: %x\n", address);
  }
}

extern "C" void mem_write(uint32_t address, uint32_t write_data, char wstrb) {
  uint8_t byte0 = (write_data >>  0) & 0xff;
  uint8_t byte1 = (write_data >>  8) & 0xff;
  uint8_t byte2 = (write_data >> 16) & 0xff;
  uint8_t byte3 = (write_data >> 24) & 0xff;
  if (address >= VGA_START && address < VGA_END) {
    address -= VGA_START;
    if (wstrb & (1<<0)) vga[address + 0] = byte0;
    if (wstrb & (1<<1)) vga[address + 1] = byte1;
    if (wstrb & (1<<2)) vga[address + 2] = byte2;
    if (wstrb & (1<<3)) vga[address + 3] = byte3;
  }
  else if (address >= MEM_START && address < MEM_END) {
    // printf("write: %u %u %u %u [%x] %u %u %u %u\n", wstrb & (1<<0), wstrb & (1<<1), wstrb & (1<<2), wstrb & (1<<3), address, byte0, byte1, byte2, byte3);
    // getchar();
    if (wstrb & (1<<0)) memory[address + 0] = byte0;
    if (wstrb & (1<<1)) memory[address + 1] = byte1;
    if (wstrb & (1<<2)) memory[address + 2] = byte2;
    if (wstrb & (1<<3)) memory[address + 3] = byte3;
  }
  else {
    printf("DUT WARNING: mem_write memory is not mapped: %x\n", address);
  }
}

extern "C" void mem_reset() {
  memset(memory, 0, MEM_SIZE);
}
