#include <stdlib.h>
#include <iostream>
#include "golden_model.cpp"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vmain.h"

void clock_tick(Trigger* clock) {
  clock->prev = clock->curr;
  clock->curr ^= 1;
}

bool compare(Vmain* dut, sCPU* gm, uint64_t sim_time) {
  bool result = true;
  int dut_v = dut->pc;
  int gm_v = gm->curr.pc.v;
  int clock = dut->clk;
  // std::cout << "at time:" << sim_time <<" clk: " << clock << " dut->pc = " << dut_v << " vs gm->pc = " << gm_v << std::endl;
  if (dut->pc != gm->curr.pc.v) {
    std::cout << "Test Failed at time " << sim_time<<" clk: " << clock  << ". PC mismatch: dut->pc = " << dut_v << " vs gm->pc = " << gm_v << std::endl;
    result = false;
  }
  for (int i = 0; i < 4; i++) {
      int dut_v = dut->regs[i];
      int gm_v = gm->curr.regs[i].v;
    // std::cout << "at time:" << sim_time << " dut->regs["<<i<<"] = " << dut_v << " vs gm->regs["<<i<<"] = " << gm_v << std::endl;
    if (dut->regs[i] != gm->curr.regs[i].v) {
      std::cout << "Test Failed at time " << sim_time << ". regs["<<i<<"] mismatch: dut->regs["<<i<<"] = " << dut_v << " vs gm->regs["<<i<<"] = " << gm_v << std::endl;
      result = false;
    }
  }
  return result;
}

int main(int argc, char** argv, char** env) {
  // NOTE: sCPU:
  inst_size_t insts[16] = {
    li(0, 10),
    li(1, 0),
    li(2, 0),
    li(3, 1),
    add(1, 1, 3),
    add(2, 2, 1),
    bner0(1, 4),
    bner0(3, 7),
  };
  sCPU cpu = {};
  for (int i = 0; i < 16; i++) {
    cpu.rom[i].v = insts[i].v;
  }
  Trigger clock = {};
  Trigger reset = {};

  // NOTE: Vmain
  Vmain* dut = new Vmain;

  const uint64_t max_sim_time = 100;
  uint64_t sim_time = 0;
  dut->clk = 0;
  while (sim_time < max_sim_time) {
    dut->eval();
    cpu_eval(&cpu, clock, reset);
    if (!compare(dut, &cpu, sim_time)) {};

    clock_tick(&clock);
    dut->clk ^= 1;
    sim_time++;
  }

  exit(EXIT_SUCCESS);
}

