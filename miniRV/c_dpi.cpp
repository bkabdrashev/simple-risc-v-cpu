#include "svdpi.h"
#include "memory.h"
#include <time.h>
#include "stdio.h"

#include "c_dpi.h"

uint64_t hash_uint64_t(uint64_t x) {
  x *= 0xff51afd7ed558ccd;
  x ^= x >> 32;
  return x;
}

uint8_t memory[MEM_SIZE];
uint8_t uart_status;
uint64_t time_uptime;

uint64_t get_time_us() {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t res = ((uint64_t)ts.tv_sec) * 1'000'000llu
       + ((uint64_t)ts.tv_nsec) / 1'000llu;
  time_uptime = res;
  return res;
}

extern "C" uint32_t mem_read(uint32_t address) {
  uint32_t result = 0;
  if (address >= MEM_START && address < MEM_END-3) {
    address -= MEM_START;
    result = 
      memory[address + 3] << 24 | memory[address + 2] << 16 |
      memory[address + 1] <<  8 | memory[address + 0] <<  0 ;
  }
  else if (address == UART_STATUS_ADDR) {
    result = uart_status != 0;
    uart_status++;
    uart_status %= 7;
    // printf("try read uart: %p, %u\n", address, result);
  }
  else if (address == TIME_UPTIME_ADDR) {
    // printf("try read uart: %p, %u\n", address, result);
    uint64_t time_us = get_time_us();
    result = (time_us & 0xffffffff);
    // printf("time low:%u\n", result);
  }
  else if (address == TIME_UPTIME_ADDR+4) {
    uint64_t time_us = get_time_us();
    result = (time_us >> 32);
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
  // printf("dut write mem: %p, 0x%x, %u\n", address, write_data, wstrb);
  // getchar();
  uint8_t byte0 = (write_data >>  0) & 0xff;
  uint8_t byte1 = (write_data >>  8) & 0xff;
  uint8_t byte2 = (write_data >> 16) & 0xff;
  uint8_t byte3 = (write_data >> 24) & 0xff;
  if (address >= MEM_START && address < MEM_END-3) {
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

uint32_t request_waited_cycles = 0;
uint64_t seed = 0;
uint32_t wait = 0;
extern "C" bool mem_request() {
  return true;
  if (request_waited_cycles == 0) {
    request_waited_cycles++;
    wait = hash_uint64_t(wait) % 10 + 5; // random wait in [5, 14] range
    return false;
  }
  else if (request_waited_cycles >= wait) {
    request_waited_cycles = 0;
    return true;
  }
  else {
    request_waited_cycles++;
    return false;
  }
}

extern "C" void mem_reset() {
  memset(memory, 0, MEM_SIZE);
}
