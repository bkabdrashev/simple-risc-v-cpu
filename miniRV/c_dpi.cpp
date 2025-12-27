#include "svdpi.h"
#include "memory.h"
#include <time.h>
#include "stdio.h"

#include "mem_map.h"

static uint8_t memory[MEM_SIZE];
static uint8_t vga[VGA_SIZE];
static uint8_t uart[UART_SIZE];
static uint32_t time_uptime[2];

static uint8_t status = 0;

uint64_t get_time_us() {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t res = ((uint64_t)ts.tv_sec) * 1'000'000llu
       + ((uint64_t)ts.tv_nsec) / 1'000llu;
  return res;
}

extern "C" uint32_t mem_read(uint32_t address) {
  uint32_t result = 0;
  if (address >= VGA_START && address < VGA_END-3) {
    address -= VGA_START;
    result = 
        vga[address + 3] << 24 | vga[address + 2] << 16 |
        vga[address + 1] <<  8 | vga[address + 0] <<  0 ;
    // printf("try read vga: %p, %u\n", address, result);
    // getchar();
  }
  else if (address >= MEM_START && address < MEM_END-3) {
    address -= MEM_START;
    result = 
      memory[address + 3] << 24 | memory[address + 2] << 16 |
      memory[address + 1] <<  8 | memory[address + 0] <<  0 ;
  }
  else if (address == UART_STATUS_ADDR) {
    result = status != 0;
    status++;
    status %= 7;
    uart[4] = result; // save uart status for gm 
    // printf("try read uart: %p, %u\n", address, result);
  }
  else if (address == TIME_UPTIME_ADDR) {
    // printf("try read uart: %p, %u\n", address, result);
    uint64_t time_us = get_time_us();
    result = (time_us & 0xffffffff);
    time_uptime[0] = result; // save time low for gm 
    // printf("time low:%u\n", result);
  }
  else if (address == TIME_UPTIME_ADDR+4) {
    uint64_t time_us = get_time_us();
    result = (time_us >> 32);
    time_uptime[1] = result; // save time high for gm 
    // printf("time:%lu\n", time_us);
  }
  else {
    // printf("DUT WARNING: mem_read memory is not mapped: %x\n", address);
    result = 0;
  }
  // printf("addr: 0x%x, res: %i\n", address, result);
  // getchar();
  return result;
}

extern "C" void mem_write(uint32_t address, uint32_t write_data, uint8_t wstrb) {
  // printf("write mem: %p, 0x%x, %u\n", address, write_data, wstrb);
  uint8_t byte0 = (write_data >>  0) & 0xff;
  uint8_t byte1 = (write_data >>  8) & 0xff;
  uint8_t byte2 = (write_data >> 16) & 0xff;
  uint8_t byte3 = (write_data >> 24) & 0xff;
  if (address >= VGA_START && address < VGA_END-3) {
    address -= VGA_START;
    if (wstrb & (1<<0)) vga[address + 0] = byte0;
    if (wstrb & (1<<1)) vga[address + 1] = byte1;
    if (wstrb & (1<<2)) vga[address + 2] = byte2;
    if (wstrb & (1<<3)) vga[address + 3] = byte3;
  }
  else if (address >= MEM_START && address < MEM_END-3) {
    address -= MEM_START;
    if (wstrb & (1<<0)) memory[address + 0] = byte0;
    if (wstrb & (1<<1)) memory[address + 1] = byte1;
    if (wstrb & (1<<2)) memory[address + 2] = byte2;
    if (wstrb & (1<<3)) memory[address + 3] = byte3;
  }
  else if (address == UART_DATA_ADDR) {
    // printf("try write uart: %p, %u, %u\n", address, write_data, wstrb);
    fputc(byte0, stderr);
  }
  else {
    // printf("DUT WARNING: mem_write memory is not mapped: %x\n", address);
  }
}

extern "C" void mem_reset() {
  memset(memory, 0, MEM_SIZE);
  memset(vga, 0, VGA_SIZE);
}
extern "C" uint64_t mem_ptr(uint64_t* out) {
  return (uint64_t)memory;
}

extern "C" uint64_t vga_ptr(uint64_t* out) {
  return (uint64_t)vga;
}

extern "C" uint64_t uart_ptr(uint64_t* out) {
  return (uint64_t)uart;
}
extern "C" uint64_t time_ptr(uint64_t* out) {
  return (uint64_t)time_uptime;
}
