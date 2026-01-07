#include <stdio.h>   // fopen, fseek, ftell, fread, fclose, fprintf
#include <stdlib.h>  // malloc, free
#include <stdint.h>  // uint8_t
#include <stddef.h>  // size_t
#include <limits.h>  // SIZE_MAX
#include <random>
#include <bitset>

#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VysyxSoCTop.h"
#include "VysyxSoCTop___024root.h"
#include "Vcpu.h"
#include "Vcpu___024root.h"

#include "riscv.cpp"
#include "gcpu.cpp"

typedef VysyxSoCTop VSoC;

struct Vcpucpu {
  uint8_t& ebreak;
  uint32_t& pc;
  uint8_t&  is_done_instruction;
  uint64_t& mcycle;
  uint64_t& minstret;
  VlUnpacked<uint32_t, 16>&  regs;

  uint8_t mem[MEM_SIZE];
  uint8_t flash[FLASH_SIZE];
  uint8_t uart[UART_SIZE];
};

struct TestBenchConfig {
  bool is_trace       = false;
  char* trace_file    = NULL;
  bool is_cycles      = false;
  bool is_bin         = false;
  char* bin_file      = NULL;
  uint64_t max_cycles = 0;

  bool is_vsoc        = false;
  bool is_vcpu        = false;
  bool is_gold        = false;
  bool is_random      = false;
  bool is_memcmp      = false;
  uint32_t seed       = 0;
  uint64_t max_tests  = 0;
  uint32_t n_insts    = 0;

};

struct TestBench {
  bool  is_trace;
  char* trace_file;
  bool  is_cycles;
  bool  is_bin;
  char* bin_file;
  uint64_t max_cycles;

  bool is_vsoc;
  bool is_vcpu;
  bool is_gold;
  bool is_random;
  bool is_memcmp;
  uint32_t seed;
  uint64_t max_tests;


  VerilatedContext* contextp;
  VSoC* vsoc;
  VerilatedVcdC* trace;

  size_t    flash_size;
  uint32_t  n_insts;
  uint32_t* insts;

  uint64_t trace_dumps;
  uint64_t reset_cycles;
  uint64_t vsoc_cycles;
  uint64_t vsoc_ticks;
  uint64_t vcpu_cycles;
  uint64_t vcpu_ticks;
  uint64_t instrets;

  VSoCcpu*  vsoc_cpu;
  Vcpucpu* vcpu_cpu;
  Vcpu* vcpu;
  Gcpu* gcpu;
};

int read_bin_file(const char* path, uint8_t** out_data, size_t* out_size) {
  if (!out_data || !out_size) return 0;

  *out_data = NULL;
  *out_size = 0;

  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Could not open %s\n", path);
    return 0;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fprintf(stderr, "Error: Could not seek to end of file.\n");
    fclose(f);
    return 0;
  }

  long len = ftell(f);
  if (len < 0) { // ftell failure
    fprintf(stderr, "Error: Could not determine file size.\n");
    fclose(f);
    return 0;
  }

  if (len == 0) { // empty file is not an error
    fclose(f);
    return 1;
  }

  // Ensure it fits into size_t
  if ((unsigned long long)len > (unsigned long long)SIZE_MAX) {
    fprintf(stderr, "Error: File too large to allocate.\n");
    fclose(f);
    return 0;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fprintf(stderr, "Error: Could not seek to start of file.\n");
    fclose(f);
    return 0;
  }

  size_t size = (size_t)len;
  uint8_t* buf = (uint8_t*)malloc(size);
  if (!buf) {
    fprintf(stderr, "Error: Out of memory.\n");
    fclose(f);
    return 0;
  }

  size_t read = fread(buf, 1, size, f);
  fclose(f);

  if (read != size) {
    fprintf(stderr, "Error: Could not read file.\n");
    free(buf);
    return 0;
  }

  *out_data = buf;
  *out_size = size;
  return 1;
}

uint64_t hash_uint64_t(uint64_t x) {
  x *= 0xff51afd7ed558ccd;
  x ^= x >> 32;
  return x;
}

bool is_valid_pc_address(uint32_t pc, uint32_t n_insts) {
  if (FLASH_START <= pc && pc <= 4*n_insts + FLASH_START) return true;
  if (MEM_START   <= pc && pc <= 4*n_insts + MEM_START)   return true;
  return false;
}

void print_all_instructions(TestBench* tb) {
  for (uint32_t i = 0; i < tb->n_insts; i++) {
    printf("[0x%08x] 0x%08x ", 4*i, tb->insts[i]);
    print_instruction(tb->insts[i]);
  }
}


uint8_t vsoc_flash[FLASH_SIZE];

extern "C" void flash_read(int32_t addr, int32_t* data) {
 *data = 
      vsoc_flash[addr + 3] << 24 | vsoc_flash[addr + 2] << 16 |
      vsoc_flash[addr + 1] <<  8 | vsoc_flash[addr + 0] <<  0 ;
}

void vsoc_flash_init(uint8_t* data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    vsoc_flash[i] = data[i];
  }
  printf("[INFO] vsoc flash written: %u bytes\n", size);
}

void vsoc_tick(TestBench* tb) {
  tb->vsoc->eval();
  if (tb->is_trace) {
    tb->trace->dump(tb->trace_dumps++);
  }
  if (tb->is_cycles && tb->vsoc_cycles % 1'000'000 == 0) printf("[INFO] vsoc cycles: %lu\n", tb->vsoc_cycles);
  tb->vsoc_ticks++;
  tb->vsoc->clock ^= 1;
}

void vsoc_cycle(TestBench* tb) {
  vsoc_tick(tb);
  vsoc_tick(tb);
  tb->vsoc_cycles++;
}

void vsoc_reset(TestBench* tb) {
  printf("[INFO] vsoc reset\n");
  tb->vsoc->reset = 1;
  tb->vsoc->clock = 0;
  for (uint64_t i = 0; i < tb->reset_cycles; i++) {
    vsoc_cycle(tb);
  }
  tb->vsoc->reset = 0;
}

void vsoc_fetch_exec(TestBench* tb) {
  while (1) {
    vsoc_cycle(tb);
    if (tb->max_cycles && tb->vsoc_cycles >= tb->max_cycles) break;
    if (tb->vsoc_cpu->ebreak) break;
    if (tb->vsoc_cpu->is_done_instruction) break;
  }
}


uint32_t v_mem_read(TestBench* tb, uint32_t addr) {
  uint32_t result = 0;
  if (addr >= FLASH_START && addr < FLASH_END-3) {
    addr &= ~3;
    addr -= FLASH_START;
    result = 
      tb->vcpu_cpu->flash[addr+3] << 24 | tb->vcpu_cpu->flash[addr+2] << 16 |
      tb->vcpu_cpu->flash[addr+1] <<  8 | tb->vcpu_cpu->flash[addr+0] <<  0 ;
  }
  else if (addr >= UART_START && addr < UART_END-3) {
    addr -= UART_START;
    uint8_t byte = 0;
    byte = tb->vcpu_cpu->uart[addr];
    result = 
      byte << 24 | byte << 16 |
      byte <<  8 | byte <<  0 ;
  }
  else if (addr >= MEM_START && addr < MEM_END-3) {
    addr &= ~3;
    addr -= MEM_START;
    result = 
      tb->vcpu_cpu->mem[addr+3] << 24 | tb->vcpu_cpu->mem[addr+2] << 16 |
      tb->vcpu_cpu->mem[addr+1] <<  8 | tb->vcpu_cpu->mem[addr+0] <<  0 ;
  }
  else {
    printf("[WARNING]: mem read  memory is not mapped 0x%x\n", addr);
  }
  return result;
}

void v_mem_write(TestBench* tb, uint8_t wen, uint8_t wbmask, uint32_t addr, uint32_t wdata) {
  if (wen) {
    if (addr >= FLASH_START && addr < FLASH_END-3) {
      // NOTE: flash is read only
      printf("[WARNING]: attempt at writing flash, which is read only\n");
    }
    else if (addr >= UART_START && addr < UART_END-3) {
      addr -= UART_START;
      uint8_t byte = 0;
      switch (addr & 0b11) {
        case 0b00 : byte = (wdata >>  0) & 0xff; break;
        case 0b01 : byte = (wdata >>  8) & 0xff; break;
        case 0b10 : byte = (wdata >> 16) & 0xff; break;
        case 0b11 : byte = (wdata >> 24) & 0xff; break;
      }
      if (addr == 0) {
        fputc(byte, stderr);
      }
      else if (addr != 5 && addr != 6) {
        tb->vcpu_cpu->uart[addr] = byte;
      }
    }
    else if (addr >= MEM_START && addr < MEM_END-3) {
      addr &= ~3;
      addr -= MEM_START;
      if (wbmask & 0b0001) tb->vcpu_cpu->mem[addr + 0] = (wdata >>  0) & 0xff;
      if (wbmask & 0b0010) tb->vcpu_cpu->mem[addr + 1] = (wdata >>  8) & 0xff;
      if (wbmask & 0b0100) tb->vcpu_cpu->mem[addr + 2] = (wdata >> 16) & 0xff;
      if (wbmask & 0b1000) tb->vcpu_cpu->mem[addr + 3] = (wdata >> 24) & 0xff;
    }
    else {
      printf("[WARNING]: mem write memory is not mapped 0x%x\n", addr);
    }
  }
}

void vcpu_flash_init(TestBench* tb, uint8_t* data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    tb->vcpu_cpu->flash[i] = data[i];
  }
  printf("[INFO] vcpu flash written: %u bytes\n", size);
}

void vcpu_tick(TestBench* tb) {
  tb->vcpu->eval();
  // if (tb->is_trace) {
  //   tb->trace->dump(tb->trace_dumps++);
  // }
  tb->vcpu_ticks++;
  tb->vcpu->clock ^= 1;
}

void vcpu_cycle(TestBench* tb) {
  vcpu_tick(tb);
  vcpu_tick(tb);
  tb->vcpu_cycles++;
  if (tb->is_cycles && tb->vcpu_cycles % 1'000'000 == 0) printf("[INFO] vcpu cycles: %lu\n", tb->vcpu_cycles);
}

void vcpu_reset(TestBench* tb) {
  printf("[INFO] vcpu reset\n");
  tb->vcpu->reset = 1;
  tb->vcpu->clock = 0;
  for (uint64_t i = 0; i < tb->reset_cycles; i++) {
    vcpu_cycle(tb);
  }
  tb->vcpu->reset = 0;

  memset(tb->vcpu_cpu->uart, 0, UART_SIZE);
  tb->vcpu_cpu->uart[1] = 0b0000'0000;
  tb->vcpu_cpu->uart[2] = 0b1100'0000;
  tb->vcpu_cpu->uart[3] = 0b0000'0011;
  tb->vcpu_cpu->uart[4] = 0b0000'0000;
  tb->vcpu_cpu->uart[5] = 0b0010'0000;
}

void vcpu_fetch_exec(TestBench* tb) {
  while (1) {
    vcpu_cycle(tb);
    tb->vcpu->io_ifu_respValid = 0;
    tb->vcpu->io_lsu_respValid = 0;

    if (tb->max_cycles && tb->vcpu_cycles >= tb->max_cycles) break;
    if (tb->vcpu_cpu->ebreak) break;
    if (tb->vcpu_cpu->is_done_instruction) break;

    if (tb->vcpu->io_ifu_reqValid) {
      tb->vcpu->io_ifu_respValid = 1;
      tb->vcpu->io_ifu_rdata = v_mem_read(tb, tb->vcpu->io_ifu_addr);
    }
    else {
      if (tb->vcpu->io_lsu_reqValid) {
        tb->vcpu->io_lsu_respValid = 1;
        v_mem_write(tb, tb->vcpu->io_lsu_wen, tb->vcpu->io_lsu_wmask, tb->vcpu->io_lsu_addr, tb->vcpu->io_lsu_wdata);
        tb->vcpu->io_lsu_rdata = v_mem_read(tb, tb->vcpu->io_lsu_addr);
      }
    }

  }
}

bool compare_reg(uint64_t sim_time, const char* name, uint32_t r, uint32_t g) {
  if (r != g) {
    printf("[FAILED] Test Failed at time %lu. %s mismatch: r = 0x%x vs g = 0x%x\n", sim_time, name, r, g);
    return false;
  }
  return true;
}

bool compare_mem(uint64_t sim_time, uint32_t address, uint32_t r, uint32_t g) {
  if (r != g) {
    printf("[FAILED] Test Failed at time %lu. 0x%x mismatch: r = %i vs g = %i\n", sim_time, address, r, g);
    return false;
  }
  return true;
}

bool compare_vsoc_gold(TestBench* tb) {
  bool result = true;
  result &= compare_reg(tb->vsoc_cycles, "EBREAK",   tb->vsoc_cpu->ebreak,   tb->gcpu->ebreak);
  result &= compare_reg(tb->vsoc_cycles, "PC",       tb->vsoc_cpu->pc,       tb->gcpu->pc);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'x', digit1, digit0, '\0'};
    result &= compare_reg(tb->vsoc_cycles, name, tb->vsoc_cpu->regs[i], tb->gcpu->regs[i]);
  }
  if (tb->is_memcmp) {
    result &= memcmp(tb->gcpu->mem, &tb->vsoc_cpu->mem.m_storage[0], MEM_SIZE) == 0;
  }
  if (!result) {
    for (uint32_t i = 0; i < MEM_SIZE; i++) {
      uint32_t v = ((uint8_t*)tb->vsoc_cpu->mem.m_storage)[i];
      uint32_t g = tb->gcpu->mem[i];
      result &= compare_mem(tb->vsoc_cycles, i, v, g);
    }
  }
  return result;
}

bool compare_vcpu_gold(TestBench* tb) {
  bool result = true;
  result &= compare_reg(tb->vcpu_cycles, "EBREAK",   tb->vcpu_cpu->ebreak,   tb->gcpu->ebreak);
  result &= compare_reg(tb->vcpu_cycles, "PC",       tb->vcpu_cpu->pc,       tb->gcpu->pc);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'x', digit1, digit0, '\0'};
    result &= compare_reg(tb->vcpu_cycles, name, tb->vcpu_cpu->regs[i], tb->gcpu->regs[i]);
  }
  if (tb->is_memcmp) {
    result &= memcmp(tb->gcpu->mem, tb->vcpu_cpu->mem, MEM_SIZE) == 0;
  }
  if (!result) {
    for (uint32_t i = 0; i < MEM_SIZE; i++) {
      uint32_t v = tb->vcpu_cpu->mem[i];
      uint32_t g = tb->gcpu->mem[i];
      result &= compare_mem(tb->vcpu_cycles, i, v, g);
    }
  }
  return result;
}

bool compare_vcpu_vsoc(TestBench* tb) {
  bool result = true;
  result &= compare_reg(tb->vsoc_cycles, "EBREAK",   tb->vcpu_cpu->ebreak,   tb->vsoc_cpu->ebreak);
  result &= compare_reg(tb->vsoc_cycles, "PC",       tb->vcpu_cpu->pc,       tb->vsoc_cpu->pc);
  result &= compare_reg(tb->vsoc_cycles, "MINSTRET", tb->vcpu_cpu->minstret, tb->vsoc_cpu->minstret);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'x', digit1, digit0, '\0'};
    result &= compare_reg(tb->vsoc_cycles, name, tb->vcpu_cpu->regs[i], tb->vsoc_cpu->regs[i]);
  }
  if (tb->is_memcmp) {
    result &= memcmp(tb->vcpu_cpu->mem, &tb->vsoc_cpu->mem.m_storage[0], MEM_SIZE) == 0;
  }
  if (!result) {
    for (uint32_t i = 0; i < MEM_SIZE; i++) {
      uint32_t v = tb->vcpu_cpu->mem[i];
      uint32_t g = tb->gcpu->mem[i];
      result &= compare_mem(tb->vsoc_cycles, i, v, g);
    }
  }
  return result;
}

bool test_instructions(TestBench* tb) {
  if (tb->is_vsoc)  {
    vsoc_reset(tb);
    vsoc_flash_init((uint8_t*)tb->insts, tb->flash_size);
  }
  if (tb->is_vcpu) {
    vcpu_reset(tb);
    vcpu_flash_init(tb, (uint8_t*)tb->insts, tb->flash_size);
  }

  if (tb->is_gold) {
    g_reset(tb->gcpu);
    g_flash_init(tb->gcpu, (uint8_t*)tb->insts, tb->flash_size);
  }

  tb->vsoc_cycles  = 0;
  tb->vcpu_cycles = 0;
  tb->instrets    = 0;
  tb->vsoc_ticks   = 0;
  tb->vcpu_ticks  = 0;

  bool is_test_success = true;
  while (1) {
    uint32_t pc = 0;
    uint32_t inst = 0;
    if (tb->is_gold) {
      pc   = tb->gcpu->pc;
      inst = g_mem_read(tb->gcpu, tb->gcpu->pc);
    }
    else if (tb->is_vcpu) {
      pc   = tb->vcpu_cpu->pc;
      inst = v_mem_read(tb, tb->vcpu_cpu->pc);
    }

    if (tb->is_gold) {
      uint8_t ebreak = cpu_eval(tb->gcpu);
      if (ebreak) {
        printf("[INFO] gcpu ebreak\n");
      }
      if (tb->gcpu->is_not_mapped) {
        break;
      }
    }

    if (tb->is_vsoc) {
      vsoc_fetch_exec(tb);
      if (tb->vsoc_cpu->ebreak) {
        printf("[INFO] vsoc ebreak\n");
        if (tb->vsoc_cpu->regs[10] != 0) {
          printf("[FAILED] test is not successful: vsoc returned %u\n", tb->vsoc_cpu->regs[10]);
          is_test_success=false;
        }
      }
      // NOTE: cycles are offset by number of cycles during the reset, since reset period is doubled for vsoc
      is_test_success &= compare_reg(tb->vsoc_cycles, "MCYCLE",   tb->vsoc_cpu->mcycle,   tb->vsoc_cycles - tb->reset_cycles);
      is_test_success &= compare_reg(tb->vsoc_cycles, "MINSTRET", tb->vsoc_cpu->minstret, tb->instrets);
    }

    if (tb->is_vcpu) {
      vcpu_fetch_exec(tb);
      if (tb->vcpu_cpu->ebreak) {
        printf("[INFO] vcpu ebreak\n");
        if (tb->vcpu_cpu->regs[10] != 0) {
          printf("[FAILED] test is not successful: vcpu returned %u\n", tb->vcpu_cpu->regs[10]);
          is_test_success=false;
        }
      }
      is_test_success &= compare_reg(tb->vcpu_cycles, "MCYCLE",   tb->vcpu_cpu->mcycle,   tb->vcpu_cycles);
      is_test_success &= compare_reg(tb->vcpu_cycles, "MINSTRET", tb->vcpu_cpu->minstret, tb->instrets);
    }

    if (tb->is_gold && tb->is_vsoc) {
      is_test_success &= compare_vsoc_gold(tb);
      if (!is_test_success) {
        printf("[%x] pc=0x%08x inst: [0x%x] ", tb->instrets, pc, inst);
        print_instruction(inst);
        break;
      }
    }

    if (tb->is_gold && tb->is_vcpu) {
      is_test_success &= compare_vcpu_gold(tb);
      if (!is_test_success) {
        printf("[%x] pc=0x%08x inst: [0x%x] ", tb->instrets, pc, inst);
        print_instruction(inst);
        break;
      }
    }

    if (!tb->is_gold && tb->is_vcpu && tb->is_vsoc) {
      is_test_success &= compare_vcpu_vsoc(tb);
      if (!is_test_success) {
        printf("[%x] pc=0x%08x inst: [0x%x] ", tb->instrets, pc, inst);
        print_instruction(inst);
        break;
      }
    }

    if (tb->max_cycles && tb->vsoc_cycles >= tb->max_cycles) {
      printf("[%x] pc=0x%08x inst: [0x%x] \n", tb->vsoc_cycles, tb->vsoc_cpu->pc);
      printf("[FAILED] test is not successful: vsoc timeout %u/%u\n", tb->vsoc_cycles, tb->max_cycles);
      is_test_success=false;
      break;
    }

    if (tb->max_cycles && tb->vcpu_cycles >= tb->max_cycles) {
      printf("[%x] pc=0x%08x inst: [0x%x] \n", tb->vcpu_cycles, tb->vcpu_cpu->pc);
      printf("[FAILED] test is not successful: vsoc timeout %u/%u\n", tb->vcpu_cycles, tb->max_cycles);
      is_test_success=false;
      break;
    }

    if (tb->is_gold && !is_valid_pc_address(tb->gcpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->is_vsoc && !is_valid_pc_address(tb->vsoc_cpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->is_vcpu && !is_valid_pc_address(tb->vcpu_cpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->instrets > tb->n_insts) {
      break;
    }
    tb->instrets++;
  }
  if (tb->is_vsoc) {
    printf("[INFO] vsoc finished:%u cycles, %u retired instructions\n", tb->vsoc_cycles, tb->vsoc_cpu->minstret);
  }
  if (tb->is_vcpu) {
    printf("[INFO] vcpu finished:%u cycles, %u retired instructions\n", tb->vcpu_cycles, tb->vcpu_cpu->minstret);
  }
  return is_test_success;
}

bool test_bin(TestBench* tb) {
  uint8_t* data = NULL; size_t size = 0;
  printf("[INFO] read file %s\n", tb->bin_file);
  int ok = read_bin_file(tb->bin_file, &data, &size);
  if (!ok) return false;

  tb->flash_size = size;
  tb->n_insts = size/4;
  tb->insts = (uint32_t*)data;

  return test_instructions(tb);
}

bool test_random(TestBench* tb) {
  tb->flash_size = tb->n_insts*4;
  tb->insts = new uint32_t[tb->n_insts];
  bool is_tests_success = true;
  uint64_t tests_passed = 0;
  uint64_t seed = 0;
  if (tb->seed) {
    seed = 13612659699927657461lu;
  }
  else {
    hash_uint64_t(std::time(0));
  }
  uint64_t i_test = 0;
  do {
    printf("======== SEED:%lu ===== %u/%u =========\n", seed, i_test, tb->max_tests);
    std::random_device rd;
    std::mt19937 gen(rd());
    gen.seed(seed);
    for (uint32_t i = 0; i < tb->n_insts; i++) {
      // uint32_t inst = random_instruction_mem_load_or_store(&gen);
      // uint32_t inst = random_instruction(&gen);
      // uint32_t inst = random_instruction_no_jump(&gen);
      uint32_t inst = random_instruction_no_mem_no_jump(&gen);
      tb->insts[i] = inst;
    }

    is_tests_success &= test_instructions(tb);
    if (is_tests_success) {
      tests_passed++;
      // print_all_instructions(tb);
    }
    else {
      print_all_instructions(tb);
    }
    seed = hash_uint64_t(seed);
    i_test++;
  } while (is_tests_success && tests_passed < tb->max_tests);

  printf("Tests results: %u / %u have passed\n", tests_passed, tb->max_tests);
  return is_tests_success;
}

static void usage(const char* prog) {
  fprintf(stderr,
    "Usage:\n"
    "  %s vsoc|vcpu|gold [trace <path>] [cycles] [memcmp] [timeout <cycles>] [seed <number>] bin    <path>\n"
    "  %s vsoc|vcpu|gold [trace <path>] [cycles] [memcmp] [timeout <cycles>] [seed <number>] random <tests> <n_insts>\n"
    "    vsoc|vcpu|gold     : select at least one to run: vsoc -- verilated SoC, vcpu -- verilated CPU, gold -- Golden Model\n"
    "    [trace <path>]     : saves the trace of the run at <path> (only for vcpu and vsoc)\n"
    "    [cycles]           : shows every 1'000'000 cycles\n"
    "    [memcmp]           : compare full memory\n"
    "    [timeout <cycles>] : timeout after <cycles> cycles\n"
    "    [seed <number>]    : set initial seed to <number>\n"
    "    random <tests> <n_insts> : <tests> times random tests with <n_insts> instructions; conflicts with bin \n"
    "    bin <path>               : loads the bin file to flash and runs it; conflicts with random \n",
    prog, prog
  );
}

static int streq(const char* a, const char* b) {
  return a && b && strcmp(a, b) == 0;
}

TestBench new_testbench(TestBenchConfig config) {
  TestBench tb = {
    .is_trace   = config.is_trace,
    .trace_file = config.trace_file,
    .is_cycles  = config.is_cycles,
    .is_bin     = config.is_bin,
    .bin_file   = config.bin_file,
    .max_cycles = config.max_cycles,

    .is_vsoc    = config.is_vsoc,
    .is_vcpu    = config.is_vcpu,
    .is_gold    = config.is_gold,

    .is_random  = config.is_random,
    .is_memcmp  = config.is_memcmp,
    .seed       = config.seed,
    .max_tests  = config.max_tests,
    .n_insts    = config.n_insts,
    .trace_dumps = 0,
    .reset_cycles = 10,
  };
  if (tb.is_trace) {
    Verilated::traceEverOn(true);
  }

  tb.vsoc = new VSoC;
  tb.vsoc_cpu = new VSoCcpu{
    .ebreak              = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__ebreak,
    .pc                  = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__pc,
    .is_done_instruction = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__is_done_instruction,
    .mcycle              = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_csr__DOT__mcycle,
    .minstret            = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_csr__DOT__minstret,
    .regs                = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_rf__DOT__regs,
    .mem                 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__sdram__DOT__mem_ext__DOT__Memory,
    .uart                = {
      .ier = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__ier,
      .iir = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__iir,
      .fcr = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__fcr,
      .mcr = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__mcr,
      .msr = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__msr,
      .lcr = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lcr,
      .lsr0 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr0r,
      .lsr1 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr1r,
      .lsr2 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr2r,
      .lsr3 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr3r,
      .lsr4 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr4r,
      .lsr5 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr5r,
      .lsr6 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr6r,
      .lsr7 = tb.vsoc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr7r,
    },
  };

  tb.vcpu = new Vcpu;
  tb.vcpu_cpu = new Vcpucpu {
    .ebreak              = tb.vcpu->rootp->cpu__DOT__ebreak,
    .pc                  = tb.vcpu->rootp->cpu__DOT__pc,
    .is_done_instruction = tb.vcpu->rootp->cpu__DOT__is_done_instruction,
    .mcycle              = tb.vcpu->rootp->cpu__DOT__u_csr__DOT__mcycle,
    .minstret            = tb.vcpu->rootp->cpu__DOT__u_csr__DOT__minstret,
    .regs                = tb.vcpu->rootp->cpu__DOT__u_rf__DOT__regs,
  };

  tb.gcpu = new Gcpu;
  tb.gcpu->vsoc_uart = &tb.vsoc_cpu->uart;

  tb.contextp = new VerilatedContext;

  if (tb.is_trace) {
    Verilated::traceEverOn(true);
    tb.trace = new VerilatedVcdC;
    if (tb.is_vsoc) {
      tb.vsoc->trace(tb.trace, 5);
    }
    if (tb.is_vcpu) {
      tb.vcpu->trace(tb.trace, 5);
    }
    tb.trace->open(tb.trace_file);
  }
  return tb;
}

void delete_testbench(TestBench tb) {
  if (tb.n_insts) {
    free(tb.insts);
  }
  if (tb.is_trace) {
    tb.trace->close();
    delete tb.trace;
  }
  delete tb.vsoc_cpu;
  delete tb.gcpu;
  delete tb.vsoc;
  delete tb.contextp;
}

int main(int argc, char** argv, char** env) {
  int exit_code = EXIT_SUCCESS;

  if (argc < 2) {
    usage(argv[0]);
    exit_code = EXIT_FAILURE;
    goto exit_label;
  }
  else {
    TestBenchConfig config = {};
    int curr_arg = 1;
    while (curr_arg < argc) {
      char* mode = argv[curr_arg++];
      if (streq(mode, "vsoc")) {
        config.is_vsoc = true;
      }
      else if (streq(mode, "gold")) {
        config.is_gold = true;
      }
      else if (streq(mode, "vcpu")) {
        config.is_vcpu = true;
      }
      else if (streq(mode, "cycles")) {
        config.is_cycles = true;
      }
      else if (streq(mode, "memcmp")) {
        config.is_memcmp = true;
      }
      else if (streq(mode, "trace")) {
        if (config.is_trace) {
          fprintf(stderr, "[ERROR]: second trace\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        if (curr_arg >= argc) {
          fprintf(stderr, "[ERROR]: 'trace' requires a <path>\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        config.trace_file = argv[curr_arg++];
        config.is_trace = true;
      }
      else if (streq(mode, "max")) {
        if (config.max_cycles) {
          fprintf(stderr, "[ERROR]: second max cycles\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        if (curr_arg >= argc) {
          fprintf(stderr, "[ERROR]: 'max' requires a <number>\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        config.max_cycles = std::stoull(argv[curr_arg++]);
      }
      else if (streq(mode, "seed")) {
        if (config.seed) {
          fprintf(stderr, "[ERROR]: second seed definition\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        if (curr_arg >= argc) {
          fprintf(stderr, "[ERROR]: 'seed' requires a <number>\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        config.seed = std::stoull(argv[curr_arg++]);
      }
      else if (streq(mode, "random")) {
        if (config.is_random) {
          fprintf(stderr, "[ERROR]: second random is not supported\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        if (curr_arg+1 >= argc) {
          fprintf(stderr, "[ERROR]: 'random' requires a <number> <number>\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        config.is_random = true;
        config.max_tests = std::stoull(argv[curr_arg++]);
        config.n_insts   = std::stoi(argv[curr_arg++]);
      }
      else if (streq(mode, "bin")) {
        if (config.is_bin) {
          fprintf(stderr, "[ERROR]: second bin is not supported\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        if (curr_arg >= argc) {
          fprintf(stderr, "[ERROR]: 'bin' requires a <path>\n");
          usage(argv[0]);
          exit_code = EXIT_FAILURE;
          goto exit_label;
        }
        config.is_bin = true;
        config.bin_file = argv[curr_arg++];
      }
      else {
        fprintf(stderr, "[ERROR]: unknown mode '%s'\n", mode);
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
      }
    }
    TestBench tb = new_testbench(config);

    if (tb.is_bin && tb.is_random) {
      printf("[WARNING] bin test and random test together are not supported: doing only bin test\n");
    }

    if (!tb.is_gold && !tb.is_vcpu && !tb.is_vsoc) {
      printf("[ERROR] should choose at least one of gold, vcpu, vsoc\n");
      usage(argv[0]);
    }

    if (tb.is_bin) {
      bool result = test_bin(&tb);
      if (!result) exit_code = EXIT_FAILURE;
    }
    else if (tb.is_random) {
      bool result = test_random(&tb);
      if (!result) exit_code = EXIT_FAILURE;
    }
    else {
      printf("[ERROR] should choose bin or random test\n");
      usage(argv[0]);
    }
    delete_testbench(tb);
  }
  
exit_label:
  return exit_code;
}


