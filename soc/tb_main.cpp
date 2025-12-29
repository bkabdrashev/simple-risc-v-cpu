#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VysyxSoCTop.h"

typedef VysyxSoCTop CPU;

#include <stdio.h>   // fopen, fseek, ftell, fread, fclose, fprintf
#include <stdlib.h>  // malloc, free
#include <stdint.h>  // uint8_t
#include <stddef.h>  // size_t
#include <limits.h>  // SIZE_MAX

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

extern "C" void flash_init(uint8_t* data, uint32_t size);

int main(int argc, char** argv, char** env) {
  VerilatedContext* contextp = new VerilatedContext;
  CPU* cpu = new CPU;
  Verilated::traceEverOn(true);
  VerilatedVcdC* m_trace = new VerilatedVcdC;
  cpu->trace(m_trace, 5);
  m_trace->open("waveform.vcd");

  uint8_t* data = NULL;
  size_t   size = 0;
  /*
   0:	555550b7          	lui	x1,0x55555
   4:	00108093          	addi	x1,x1,1 # 0x55555001
   8:	80000137          	lui	x2,0x80000
   c:	00410113          	addi	x2,x2,4 # 0x80000004
  10:	00112023          	sw	x1,0(x2)
  14:	00012183          	lw	x3,0(x2)
  18:	00100073          	ebreak
    */
  // read_bin_file("code2.bin", &data, &size);
  // read_bin_file("hello-minirv-ysyxsoc.bin", &data, &size);
  read_bin_file("new.bin", &data, &size);
  flash_init(data, (uint32_t)size);

  uint64_t max_sim_time = 0;
  uint64_t trace_period = 150;
  cpu->reset = 1;

  cpu->clock = 0;
  cpu->eval();
  uint64_t counter = 0;
  m_trace->dump(counter++);

  cpu->clock = 1;
  cpu->eval();
  m_trace->dump(counter++);

  cpu->clock = 0;
  cpu->reset = 0;

  for (uint64_t t = 0; (!max_sim_time || t < max_sim_time) && !contextp->gotFinish(); t++) {
    cpu->eval();
    cpu->clock ^= 1;
    if (t % trace_period == 0) {
      m_trace->dump(counter++);
    }
  }
  printf("exit\n");

  m_trace->close();
  int exit_code = EXIT_SUCCESS;
  return exit_code;
}


