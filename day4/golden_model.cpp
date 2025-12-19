#include <cstdint>
#include <assert.h>
#include <iostream>

#define ERROR_NOT_IMPLEMENTED (0b10100001)
#define ERROR_INVALID_RANGE   (0b10100010)
#define OPCODE_BITS 2u
#define REG_INDEX_BITS 2u
#define ADDR_BITS 4u
#define OPCODE_IMM_BITS 4u
#define REG_BITS 8u
#define INST_BITS 8u
#define ROM_SIZE (1u << ADDR_BITS)
#define N_REGS (1u << REG_INDEX_BITS)
#define N_INSTS (1u << OPCODE_BITS)

struct bit {
  uint8_t v : 1;
};
struct Trigger {
  uint8_t prev : 1;
  uint8_t curr : 1;
};
struct opcode_size_t {
  uint8_t v : OPCODE_BITS;
};
struct addr_size_t {
  uint8_t v : ADDR_BITS;
};
struct imm_size_t {
  uint8_t v : OPCODE_IMM_BITS;
};
struct reg_index_t {
  uint8_t v : REG_BITS;
};
struct reg_size_t {
  uint8_t v : REG_BITS;
};
struct inst_size_t {
  uint8_t v : REG_BITS;
};

struct sCPU {
  struct {
    addr_size_t pc;
    reg_size_t regs[N_REGS];
  } curr;
  struct {
    addr_size_t pc;
    reg_size_t regs[N_REGS];
    reg_size_t write_data;
  } prev;
  reg_size_t rom[ROM_SIZE];
};

struct ALU_out {
  reg_size_t result;
  bit is_ner0;
};

uint8_t take_bit(uint8_t bits, uint8_t pos) {
  if (pos > 7) {
    assert(0);
    return ERROR_INVALID_RANGE;
  }

  uint8_t mask = (1u << pos);
  return (bits & mask) >> pos;
}

uint8_t take_bits_range(uint8_t bits, uint8_t from, uint8_t to) {
  if (to > 7 || from > 7 || from > to) {
    return ERROR_INVALID_RANGE;
  }

  uint8_t mask = ((1u << (to - from + 1)) - 1u) << from;
  return (bits & mask) >> from;
}

bool is_positive_edge(Trigger trigger) {
  if (!trigger.prev && trigger.curr) {
    return true;
  }
  return false;
}

bool is_negative_edge(Trigger trigger) {
  if (trigger.prev && !trigger.curr) {
    return true;
  }
  return false;
}

ALU_out alu_eval(opcode_size_t opcode, reg_size_t r0, reg_size_t rdata1, reg_size_t rdata2, reg_size_t imm) {
  ALU_out result = {};
  if (opcode.v == 0b00) {
    result.result.v = rdata1.v + rdata2.v;
  }
  else if (opcode.v == 0b01) {
    result.result.v = ERROR_NOT_IMPLEMENTED;
  }
  else if (opcode.v == 0b10) {
    result.result.v = imm.v;
  }
  else if (opcode.v == 0b11) {
    result.result.v = ERROR_NOT_IMPLEMENTED;
  }
  result.is_ner0.v = r0.v != rdata2.v;
  int a = r0.v;
  int b = rdata2.v;
  return result;
}

inst_size_t rom_eval(sCPU* cpu, addr_size_t addr) {
  inst_size_t result = {};
  result.v = cpu->rom[addr.v].v;
  return result;
}

addr_size_t pc_eval(sCPU* cpu, Trigger clock, Trigger reset, addr_size_t in_addr, bit is_jump) {
  int a = is_jump.v;
  cpu->prev.pc.v = cpu->curr.pc.v;
  if (is_positive_edge(reset)) {
    cpu->curr.pc.v = 0;
  }
  else if (is_positive_edge(clock)) {
    if (is_jump.v) {
      cpu->curr.pc.v = in_addr.v;
    }
    else {
      cpu->curr.pc.v++;
    }
  }
  return cpu->curr.pc;
}

struct RF_out {
  reg_size_t rdata1;
  reg_size_t rdata2;
  reg_size_t regs[N_REGS];
};

void copy_curr_to_prev(sCPU* cpu) {
  cpu->prev.pc.v = cpu->curr.pc.v;
  for (int i = 0; i < N_REGS; i++) {
    cpu->prev.regs[i].v = cpu->curr.regs[i].v;
  }
}

RF_out rf_eval(sCPU* cpu, Trigger clock, Trigger reset, bit write_enable, reg_index_t reg_dest, reg_index_t reg_src1, reg_index_t reg_src2, reg_size_t write_data) {
  RF_out out = {};
  if (is_positive_edge(reset)) {
    cpu->curr.regs[0].v = 0;
    cpu->curr.regs[1].v = 0;
    cpu->curr.regs[2].v = 0;
    cpu->curr.regs[3].v = 0;
  }
  else if (is_positive_edge(clock)) {
    if (write_enable.v) {
      cpu->curr.regs[reg_dest.v] = write_data;
    }
  }
  out.rdata1.v = cpu->prev.regs[reg_src1.v].v;
  out.rdata2.v = cpu->prev.regs[reg_src2.v].v;
  for (int i = 0; i < N_REGS; i++) {
    out.regs[i].v = cpu->prev.regs[i].v;
  }
  return out;
}

struct Dec_out {
  reg_index_t reg_dest;
  reg_index_t reg_src1;
  reg_index_t reg_src2;
  imm_size_t imm;
  opcode_size_t opcode;
  bit write_enable;
  addr_size_t addr;
};

Dec_out dec_eval(inst_size_t inst) {
  Dec_out out = {};
  out.reg_dest.v = take_bits_range(inst.v, 4, 5);
  out.reg_src1.v = take_bits_range(inst.v, 2, 3);
  out.reg_src2.v = take_bits_range(inst.v, 0, 1);
  out.imm.v = take_bits_range(inst.v, 0, 3);
  out.addr.v = take_bits_range(inst.v, 2, 5);
  out.opcode.v = take_bits_range(inst.v, 6, 7);
  bit opcode0 = { take_bits_range(inst.v, 6, 6) };
  bit opcode1 = { take_bits_range(inst.v, 6, 6) };
  out.write_enable.v = ~(opcode0.v & opcode1.v);
  return out;
}

void cpu_eval(sCPU* cpu, Trigger clock, Trigger reset) {
  copy_curr_to_prev(cpu);
  inst_size_t inst = rom_eval(cpu, cpu->prev.pc);
  Dec_out dec_out = dec_eval(inst);
  RF_out rf_out = rf_eval(cpu, clock, reset, dec_out.write_enable, dec_out.reg_dest, dec_out.reg_src1, dec_out.reg_src2, cpu->prev.write_data);
  uint8_t extend = (0 << 4) | dec_out.imm.v;
  reg_size_t imm_extend = { extend };
  ALU_out alu_out = alu_eval(dec_out.opcode, cpu->prev.regs[0], rf_out.rdata1, rf_out.rdata2, imm_extend);
  cpu->prev.write_data = alu_out.result;
  uint8_t opcode_bit_0 = take_bit(dec_out.opcode.v, 0);
  uint8_t opcode_bit_1 = take_bit(dec_out.opcode.v, 1);
  // TODO: check for overflow
  bit opcode0 = { opcode_bit_0 };
  bit opcode1 = { opcode_bit_1 };
  uint8_t is_jump_v = opcode0.v & opcode1.v & alu_out.is_ner0.v & ~reset.curr;
  bit is_jump = { is_jump_v };
  pc_eval(cpu, clock, reset, dec_out.addr, is_jump);
}

inst_size_t inst_add(reg_index_t reg_dest, reg_index_t reg_src1, reg_index_t reg_src2) {
  uint8_t bits = (0 << 7) | (0 << 6) | (reg_dest.v << 4) | (reg_src1.v << 2) | reg_src2.v;
  inst_size_t add = { bits };
  return add;
}

inst_size_t add(uint8_t reg_dest, uint8_t reg_src1, uint8_t reg_src2) {
  // TODO: check for fit
  reg_index_t rd = {reg_dest};
  reg_index_t rs1 = {reg_src1};
  reg_index_t rs2 = {reg_src2};
  return inst_add(rd, rs1, rs2);
}

inst_size_t inst_li(reg_index_t reg_dest, imm_size_t imm) {
  uint8_t bits = (1 << 7) | (0 << 6) | (reg_dest.v << 4) | imm.v;
  inst_size_t li = { bits };
  return li;
}

inst_size_t li(uint8_t reg_dest, uint8_t imm_u8) {
  // TODO: check for fit
  reg_index_t rd = {reg_dest};
  imm_size_t  imm = {imm_u8};
  return inst_li(rd, imm);
}

inst_size_t inst_bner0(addr_size_t addr, reg_index_t reg_src2) {
  uint8_t bits = (1 << 7) | (1 << 6) | (addr.v << 2) | reg_src2.v;
  inst_size_t bner0 = { bits };
  return bner0;
}

inst_size_t bner0(uint8_t reg_src2, uint8_t addr_u8) {
  // TODO: check for fit
  addr_size_t addr = {addr_u8};
  reg_index_t  rs2 = {reg_src2};
  return inst_bner0(addr, rs2);
}

