#include <stdlib.h>
#include <iostream>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vmain.h"

#define MAX_SIM_TIME 100

void reset(Vmain* dut) {
  dut->reset = 1;
  dut->eval();
  dut->reset = 0;
  dut->eval();
  dut->clk = 0;
}

int main(int argc, char** argv, char** env) {
    Vmain *dut = new Vmain;

    int total_tests = 13;
    int passed_count = 0;
    std::cout<<"=========== Test start: " << total_tests << " total tests ============" << std::endl;

    { // Reset test
      reset(dut);
      dut->clk = 0;
      dut->eval();
      if (dut->pc != 0) {
        int got = dut->pc;
        std::cout << "Test Failed:" << " PC expected 0 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[0] != 0) {
        int got = dut->regs[0];
        std::cout << "Test Failed:" << " Reg0 expected 0 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[1] != 0) {
        int got = dut->regs[1];
        std::cout << "Test Failed:" << " Reg1 expected 0 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[2] != 0) {
        int got = dut->regs[2];
        std::cout << "Test Failed:" << " Reg2 expected 0 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[3] != 0) {
        int got = dut->regs[3];
        std::cout << "Test Failed:" << " Reg[3] expected 0 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
    }

    { // PC test
      reset(dut);
      dut->clk = 0;
      dut->eval();
      if (dut->pc != 0) {
        int pc = dut->pc;
        std::cout << "Test Failed:" << " PC expected 0 but got " << pc << std::endl;
      }
      else {
        passed_count++;
      }
    }

    { // PC test
      reset(dut);
      dut->clk = 0;
      dut->eval();
      dut->clk = 1;
      dut->eval();
      dut->clk = 0;
      dut->eval();
      dut->clk = 1;
      dut->eval();
      if (dut->pc != 2) {
        int pc = dut->pc;
        std::cout << "Test Failed:" << " PC expected 0 but got " << pc << std::endl;
      }
      else {
        passed_count++;
      }
    }

    { // Reg0 test
      reset(dut);
      dut->clk = 0;
      dut->eval();
      dut->clk = 1;
      dut->eval();
      if (dut->regs[0] != 10) {
        int got = dut->regs[0];
        std::cout << "Test Failed:" << " Reg0 expected 10 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
    }

    { // All Regs test
      reset(dut);
      vluint64_t sim_time = 0;
      while (sim_time < MAX_SIM_TIME) {
        dut->clk ^= 1;
        dut->eval();
        sim_time++;
      }
      if (dut->pc != 7) {
        int got = dut->pc;
        std::cout << "Test Failed:" << " PC expected 7 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[0] != 10) {
        int got = dut->regs[0];
        std::cout << "Test Failed:" << " Reg0 expected 10 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[1] != 10) {
        int got = dut->regs[1];
        std::cout << "Test Failed:" << " Reg1 expected 10 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[2] != 55) {
        int got = dut->regs[2];
        std::cout << "Test Failed:" << " Reg2 expected 55 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
      if (dut->regs[3] != 1) {
        int got = dut->regs[3];
        std::cout << "Test Failed:" << " Reg[3] expected 1 but got " << got << std::endl;
      }
      else {
        passed_count++;
      }
    }

    std::cout<<"=========== Test end: "<< passed_count << " / " << total_tests << " passed ============" << std::endl;


    {
      reset(dut);
      Verilated::traceEverOn(true);
      VerilatedVcdC *m_trace = new VerilatedVcdC;
      dut->trace(m_trace, 5);
      m_trace->open("waveform.vcd");

      vluint64_t sim_time = 0;
      while (sim_time < MAX_SIM_TIME) {
        dut->clk ^= 1;
        dut->eval();
        m_trace->dump(sim_time);
        sim_time++;
      }

      m_trace->close();
    }
    delete dut;
    exit(EXIT_SUCCESS);
}

