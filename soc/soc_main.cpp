#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VysyxSoCTop.h"
#include "VysyxSoCTop___024root.h"
#include "Vcpu.h"
#include "Vcpu___024root.h"

typedef VysyxSoCTop SoC;

#include <stdio.h>   // fopen, fseek, ftell, fread, fclose, fprintf
#include <stdlib.h>  // malloc, free
#include <stdint.h>  // uint8_t
#include <stddef.h>  // size_t
#include <limits.h>  // SIZE_MAX
#include <random>
#include <bitset>

#include "riscv.cpp"
#include "gcpu.cpp"

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

extern void flash_init(uint8_t* data, uint32_t size);

struct TestBenchConfig {
  bool is_trace       = false;
  char* trace_file    = NULL;
  bool is_cycles      = false;
  bool is_bin         = false;
  char* bin_file      = NULL;
  uint64_t max_cycles = 0;

  bool is_diff        = false;
  bool is_random      = false;
  uint64_t max_tests  = 0;
  uint32_t  n_insts   = 0;
};

struct TestBench {
  bool  is_trace;
  char* trace_file;
  bool  is_cycles;
  bool  is_bin;
  char* bin_file;
  uint64_t max_cycles;

  bool is_diff;
  bool is_random;
  uint64_t max_tests;

  VerilatedContext* contextp;
  SoC* soc;
  VerilatedVcdC* trace;

  size_t    flash_size;
  uint32_t  n_insts;
  uint32_t* insts;

  uint64_t trace_dumps;
  uint64_t reset_cycles;
  uint64_t cycles;
  uint64_t ticks;
  uint64_t instrets;

  SoCcpu* soc_cpu;
  Vcpu* vcpu;
  Gcpu* gcpu;
};

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

void soc_tick(TestBench* tb) {
  tb->soc->eval();
  if (tb->is_trace) {
    tb->trace->dump(tb->trace_dumps++);
  }
  if (tb->is_cycles && tb->cycles % 1'000'000 == 0) printf("[INFO] cycles: %lu\n", tb->cycles);
  tb->ticks++;
  tb->soc->clock ^= 1;
}

void soc_cycle(TestBench* tb) {
  soc_tick(tb);
  soc_tick(tb);
  tb->cycles++;
}

void soc_reset(TestBench* tb) {
  printf("[INFO] reset\n");
  tb->soc->reset = 1;
  tb->soc->clock = 0;
  for (uint64_t i = 0; i < tb->reset_cycles; i++) {
    soc_cycle(tb);
  }
  tb->soc->reset = 0;
}


void soc_fetch_exec(TestBench* tb) {
  while (1) {
    soc_cycle(tb);
    if (tb->max_cycles && tb->cycles >= tb->max_cycles) break;
    if (tb->soc_cpu->ebreak) break;
    if (tb->soc_cpu->is_done_instruction) break;
  }
}

void cpu_fetch_exec(TestBench* tb) {
  while (1) {
    cpu_cycle(tb);
    if (tb->max_cycles && tb->cycles >= tb->max_cycles) break;
    if (tb->soc_cpu->ebreak) break;
    if (tb->soc_cpu->is_done_instruction) break;
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

bool compare(TestBench* tb) {
  bool result = true;
  result &= compare_reg(tb->cycles, "EBREAK",   tb->soc_cpu->ebreak,   tb->gcpu->ebreak);
  result &= compare_reg(tb->cycles, "PC",       tb->soc_cpu->pc,       tb->gcpu->pc);
  result &= compare_reg(tb->cycles, "MCYCLE",   tb->soc_cpu->mcycle,   tb->cycles - tb->reset_cycles); // NOTE: cycles are offset by number of cycles during the reset, since reset period is doubled
  result &= compare_reg(tb->cycles, "MINSTRET", tb->soc_cpu->minstret, tb->instrets);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'x', digit1, digit0, '\0'};
    result &= compare_reg(tb->cycles, name, tb->soc_cpu->regs[i], tb->gcpu->regs[i]);
  }
  // result &= memcmp(tb->gcpu->flash, tb->vflash, FLASH_SIZE) == 0;
  result &= memcmp(tb->gcpu->mem, &tb->soc_cpu->mem.m_storage[0], MEM_SIZE) == 0;
  if (!result) {
    for (uint32_t i = 0; i < MEM_SIZE; i++) {
      uint32_t v = ((uint8_t*)tb->soc_cpu->mem.m_storage)[i];
      uint32_t g = tb->gcpu->mem[i];
      result &= compare_mem(tb->cycles, i, v, g);
    }
    // for (uint32_t i = 0; i < FLASH_SIZE; i++) {
    //   uint32_t v = tb->vflash[i];
    //   uint32_t g = tb->gcpu->flash[i];
    //   result &= compare_mem(tb->cycles, i, v, g);
    // }
  }
  return result;
}

bool test_instructions(TestBench* tb) {
  v_reset(tb);

  flash_init((uint8_t*)tb->insts, tb->flash_size);
  if (tb->is_diff) {
    g_reset(tb->gcpu);
    g_flash_init(tb->gcpu, (uint8_t*)tb->insts, tb->flash_size);
  }

  tb->cycles   = 0;
  tb->instrets = 0;
  tb->ticks    = 0;

  bool is_test_success = true;
  while (1) {
    uint32_t pc = tb->gcpu->pc;
    uint32_t inst = g_mem_read(tb->gcpu, tb->gcpu->pc);
    if (tb->is_gold) {
      uint8_t ebreak = cpu_eval(tb->gcpu);
      if (ebreak) {
        printf("[INFO] gcpu ebreak\n");
      }
      if (tb->gcpu->is_not_mapped) {
        break;
      }
    }

    if (tb->is_soc) {
      soc_fetch_exec(tb);
      if (tb->soc_cpu->ebreak) {
        printf("[INFO] soc ebreak\n");
        if (!tb->is_diff && tb->soc_cpu->regs[10] != 0) {
          printf("[FAILED] test is not successful: soc_cpu returned %u\n", tb->soc_cpu->regs[10]);
          is_test_success=false;
        }
      }
    }

    if (tb->is_gold) {
      is_test_success &= compare(tb);
      if (!is_test_success) {
        printf("[%x] pc=0x%08x inst: [0x%x] ", tb->cycles, pc, inst);
        print_instruction(inst);
        break;
      }
    }

    if (tb->is_vcpu) {
      is_test_success &= compare(tb);
      if (!is_test_success) {
        printf("[%x] pc=0x%08x inst: [0x%x] ", tb->cycles, pc, inst);
        print_instruction(inst);
        break;
      }
    }

    if (tb->max_cycles && tb->cycles >= tb->max_cycles) {
      printf("[%x] pc=0x%08x inst: [0x%x] \n", tb->cycles, tb->soc_cpu->pc);
      printf("[FAILED] test is not successful: timeout %u/%u\n", tb->cycles, tb->max_cycles);
      is_test_success=false;
      break;
    }

    if (tb->is_gold && !is_valid_pc_address(tb->gcpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->is_soc && !is_valid_pc_address(tb->soc_cpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->is_vcpu && !is_valid_pc_address(tb->vcpu->pc, tb->n_insts)) {
      break;
    }
    if (tb->instrets > tb->n_insts) {
      break;
    }
    tb->instrets++;
  }
  printf("[INFO] finished:%u cycles, %u retired instructions\n", tb->cycles, tb->instrets);
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
  uint64_t seed = hash_uint64_t(std::time(0));
  // uint64_t seed = 11178771775999776808lu;
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
    "  %s [diff] [trace <path>] [cycles] [max <cycles>] bin    <path>\n"
    "  %s [diff] [trace <path>] [cycles] [max <cycles>] random <tests> <n_insts>\n",
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
    .is_diff    = config.is_diff,
    .is_random  = config.is_random,
    .max_tests  = config.max_tests,
    .n_insts    = config.n_insts,
    .trace_dumps = 0,
    .reset_cycles = 10,
  };
  if (tb.is_trace) {
    Verilated::traceEverOn(true);
  }

  tb.soc = new SoC;
  tb.soc_cpu = new SoCcpu{
    .ebreak              = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__ebreak,
    .pc                  = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__pc,
    .is_done_instruction = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__is_done_instruction,
    .mcycle              = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_csr__DOT__mcycle,
    .minstret            = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_csr__DOT__minstret,
    .regs                = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__cpu__DOT__u_cpu__DOT__u_rf__DOT__regs,
    .mem                 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__sdram__DOT__mem_ext__DOT__Memory,
    .uart                = {
      .ier = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__ier,
      .iir = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__iir,
      .fcr = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__fcr,
      .mcr = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__mcr,
      .msr = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__msr,
      .lcr = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lcr,
      .lsr0 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr0r,
      .lsr1 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr1r,
      .lsr2 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr2r,
      .lsr3 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr3r,
      .lsr4 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr4r,
      .lsr5 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr5r,
      .lsr6 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr6r,
      .lsr7 = tb.soc->rootp->ysyxSoCTop__DOT__dut__DOT__asic__DOT__luart__DOT__muart__DOT__Uregs__DOT__lsr7r,
    },
  };

  tb.vcpu = new Vcpu;

  tb.gcpu = new Gcpu;
  tb.gcpu->vuart = &tb.soc_cpu->uart;

  tb.contextp = new VerilatedContext;

  if (tb.is_trace) {
    Verilated::traceEverOn(true);
    tb.trace = new VerilatedVcdC;
    tb.soc->trace(tb.trace, 5);
    tb.vcpu->trace(tb.trace, 5);
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
  delete tb.soc_cpu;
  delete tb.gcpu;
  delete tb.soc;
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
      if (streq(mode, "trace")) {
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
      else if (streq(mode, "diff")) {
        config.is_diff = true;
      }
      else if (streq(mode, "cycles")) {
        config.is_cycles = true;
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
        config.max_cycles = atoi(argv[curr_arg++]);
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
        config.max_tests = atoi(argv[curr_arg++]);
        config.n_insts   = atoi(argv[curr_arg++]);
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
    if (tb.is_bin) {
      bool result = test_bin(&tb);
      if (!result) exit_code = EXIT_FAILURE;
    }
    else if (tb.is_random) {
      bool result = test_random(&tb);
      if (!result) exit_code = EXIT_FAILURE;
    }
    delete_testbench(tb);
  }
  
exit_label:
  return exit_code;
}


