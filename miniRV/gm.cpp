#include <cstdint>
#include <assert.h>

#define ERROR_NOT_IMPLEMENTED (1010)
#define ERROR_INVALID_RANGE   (2020)
#define OPCODE_BITS 7u
#define FUNCT3_BITS 3u
#define REG_INDEX_BITS 5u
#define ADDR_BITS 32u
#define REG_BITS 32u
#define BYTE 8u
#define INST_BITS 32u
#define MEM_START (0x00000000)
#define MEM_END  (128 * 1024 * 1024)
#define MEM_SIZE (MEM_END - MEM_START)
#define VGA_START (0x20000000)
#define VGA_END  (0x20040000)
#define VGA_SIZE (VGA_END - VGA_START)
#define N_REGS 16u

#define OPCODE_ADDI  (0b0010011)
#define FUNCT3_ADDI  (0b000)
#define OPCODE_JALR  (0b1100111)
#define FUNCT3_JALR  (0b000)
#define OPCODE_ADD   (0b0110011)
#define FUNCT3_ADD   (0b000)
#define OPCODE_LUI   (0b0110111)
#define FUNCT3_LUI   (0b000)
#define OPCODE_LW    (0b0000011)
#define OPCODE_LBU   (OPCODE_LW)
#define FUNCT3_LW    (0b010)
#define FUNCT3_LBU   (0b100)
#define OPCODE_SW    (0b0100011)
#define OPCODE_SB    (OPCODE_SW)
#define FUNCT3_SW    (0b010)
#define FUNCT3_SB    (0b000)
#define INST(name) (((FUNCT3_##name) << OPCODE_BITS) | (OPCODE_##name))

struct bit {
  uint32_t v : 1;
};
struct bit4 {
  union {
    struct {
      bit a;
      bit b;
      bit c;
      bit d;
    };
    bit bits[4];
  };
};
struct Trigger {
  uint32_t prev : 1;
  uint32_t curr : 1;
};
struct opcode_size_t {
  uint32_t v : OPCODE_BITS;
};
struct funct3_size_t {
  uint32_t v : OPCODE_BITS;
};
struct addr_size_t {
  uint32_t v : ADDR_BITS;
};
struct reg_index_t {
  uint32_t v : REG_BITS;
};
struct reg_size_t {
  uint32_t v : REG_BITS;
};
struct byte {
  uint8_t v : BYTE;
};

typedef reg_size_t inst_size_t;

bit ZERO_BIT = {};
reg_index_t REG_0 = {};
reg_size_t REG_VALUE_0 = {};

struct miniRV {
  addr_size_t pc;
  reg_size_t regs[N_REGS];
  byte mem[MEM_SIZE];
  byte vga[VGA_SIZE];
};

int32_t sar32(uint32_t u, unsigned shift) {
  assert(shift < 32);

  if (shift == 0) return (u & 0x80000000u) ? -int32_t((~u) + 1u) : int32_t(u);

  uint32_t shifted = u >> shift;      
  if (u & 0x80000000u) {
    shifted |= (~0u << (32 - shift)); 
  }
  return static_cast<int32_t>(shifted);
}

uint32_t take_bit(uint32_t bits, uint32_t pos) {
  if (pos >= REG_BITS) {
    assert(0);
    return ERROR_INVALID_RANGE;
  }

  uint32_t mask = (1u << pos);
  return (bits & mask) >> pos;
}

uint32_t take_bits_range(uint32_t bits, uint32_t from, uint32_t to) {
  if (to >= REG_BITS || from >= REG_BITS || from > to) {
    assert(0);
    return ERROR_INVALID_RANGE;
  }

  uint32_t mask = ((1u << (to - from + 1)) - 1u) << from;
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

reg_size_t alu_eval(opcode_size_t opcode, reg_size_t rdata1, reg_size_t rdata2, reg_size_t imm) {
  reg_size_t result = {};
  if (opcode.v == OPCODE_ADDI) {
    // ADDI
    result.v = rdata1.v + imm.v;
  }
  else if (opcode.v == OPCODE_ADD) {
    result.v = rdata1.v + rdata2.v;
  }
  else {
    result.v = ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

void gm_mem_reset(miniRV* cpu) {
  memset(cpu->mem, 0, MEM_SIZE);
  memset(cpu->vga, 0, VGA_SIZE);
}

inst_size_t gm_mem_read(miniRV* cpu, addr_size_t addr) {
  inst_size_t result = {0};
  if (addr.v >= VGA_START && addr.v < VGA_END) {
    addr.v -= VGA_START;
    result.v = 
      cpu->vga[addr.v+3].v << 24 | cpu->vga[addr.v+2].v << 16 |
      cpu->vga[addr.v+1].v <<  8 | cpu->vga[addr.v+0].v <<  0 ;
  }
  else if (addr.v >= MEM_START && addr.v < MEM_END) {
    result.v = 
      cpu->mem[addr.v+3].v << 24 | cpu->mem[addr.v+2].v << 16 |
      cpu->mem[addr.v+1].v <<  8 | cpu->mem[addr.v+0].v <<  0 ;
  }
  else {
    printf("GM WARNING: memory is not mapped\n");
  }
  return result;
}

void gm_mem_write(miniRV* cpu, Trigger clock, Trigger reset, bit write_enable, bit4 write_enable_bytes, addr_size_t addr, reg_size_t write_data) {
  if (is_positive_edge(reset)) {
    memset(cpu->mem, 0, MEM_SIZE);
    memset(cpu->vga, 0, VGA_SIZE);
  }
  else if (is_positive_edge(clock)) {
    if (write_enable.v) {
      if (addr.v >= VGA_START && addr.v < VGA_END) {
        addr.v -= VGA_START;
        if (write_enable_bytes.bits[0].v) {
          cpu->vga[addr.v + 0].v = (write_data.v & (0xff << 0)) >> 0;
        }
        if (write_enable_bytes.bits[1].v) {
          cpu->vga[addr.v + 1].v = (write_data.v & (0xff << 8)) >> 8;
        }
        if (write_enable_bytes.bits[2].v) {
          cpu->vga[addr.v + 2].v = (write_data.v & (0xff << 16)) >> 16;
        }
        if (write_enable_bytes.bits[3].v) {
          cpu->vga[addr.v + 3].v = (write_data.v & (0xff << 24)) >> 24;
        }
      }
      else if (addr.v >= MEM_START && addr.v < MEM_END) {
        if (write_enable_bytes.bits[0].v) {
          cpu->mem[addr.v + 0].v = (write_data.v & (0xff << 0)) >> 0;
        }
        if (write_enable_bytes.bits[1].v) {
          cpu->mem[addr.v + 1].v = (write_data.v & (0xff << 8)) >> 8;
        }
        if (write_enable_bytes.bits[2].v) {
          cpu->mem[addr.v + 2].v = (write_data.v & (0xff << 16)) >> 16;
        }
        if (write_enable_bytes.bits[3].v) {
          cpu->mem[addr.v + 3].v = (write_data.v & (0xff << 24)) >> 24;
        }
      }
      else {
        printf("GM WARNING: memory is not mapped\n");
      }
    }
  }
}

void pc_write(miniRV* cpu, Trigger clock, Trigger reset, addr_size_t in_addr, bit is_jump) {
  if (is_positive_edge(reset)) {
    cpu->pc.v = 0;
  }
  else if (is_positive_edge(clock)) {
    if (is_jump.v) {
      cpu->pc.v = in_addr.v;
    }
    else {
      cpu->pc.v += 4;
    }
  }
}

struct RF_out {
  reg_size_t rdata1;
  reg_size_t rdata2;
  reg_size_t regs[N_REGS];
};

RF_out rf_read(miniRV* cpu, reg_index_t reg_src1, reg_index_t reg_src2) {
  RF_out out = {};
  out.rdata1.v = cpu->regs[reg_src1.v].v;
  out.rdata2.v = cpu->regs[reg_src2.v].v;
  for (int i = 0; i < N_REGS; i++) {
    out.regs[i].v = cpu->regs[i].v;
  }
  return out;
}

void rf_write(miniRV* cpu, Trigger clock, Trigger reset, bit write_enable, reg_index_t reg_dest, reg_size_t write_data) {
  if (is_positive_edge(reset)) {
    for (uint32_t i = 0; i < N_REGS; i++) {
      cpu->regs[i].v = 0;
    }
  }
  else if (is_positive_edge(clock)) {
    if (write_enable.v && reg_dest.v != 0) {
      cpu->regs[reg_dest.v] = write_data;
    }
  }
}

struct Dec_out {
  reg_index_t reg_dest;
  reg_index_t reg_src1;
  reg_index_t reg_src2;
  reg_size_t imm;
  opcode_size_t opcode;
  funct3_size_t funct3;
  bit write_enable;
};

Dec_out dec_eval(inst_size_t inst) {
  Dec_out out = {};
  out.opcode.v = take_bits_range(inst.v, 0, 6);
  out.reg_dest.v = take_bits_range(inst.v, 7, 11);
  out.funct3.v = take_bits_range(inst.v, 12, 14);
  out.reg_src1.v = take_bits_range(inst.v, 15, 19);
  out.reg_src2.v = take_bits_range(inst.v, 20, 24);
  bit sign = {.v=take_bit(inst.v, 31)};
  if (out.opcode.v == OPCODE_ADDI) {
    // ADDI
    if (sign.v) out.imm.v = (-1 << 12) | take_bits_range(inst.v, 20, 31);
    else        out.imm.v = ( 0 << 12) | take_bits_range(inst.v, 20, 31);
    out.write_enable.v = 1;
  }
  else if (out.opcode.v == OPCODE_JALR) {
    // JALR
    if (sign.v) out.imm.v = (-1 << 12) | take_bits_range(inst.v, 20, 31);
    else        out.imm.v = ( 0 << 12) | take_bits_range(inst.v, 20, 31);
    out.write_enable.v = 1;
  }
  else if (out.opcode.v == OPCODE_ADD) {
    // ADD
    if (sign.v) out.imm.v = (-1 << 20) | take_bits_range(inst.v, 12, 31);
    else        out.imm.v = ( 0 << 20) | take_bits_range(inst.v, 12, 31);
    out.write_enable.v = 1;
  }
  else if (out.opcode.v == OPCODE_LUI) {
    // LUI
    out.imm.v = take_bits_range(inst.v, 12, 31) << 12;
    out.write_enable.v = 1;
  }
  else if (out.opcode.v == OPCODE_LW) {
    // LW, LBU
    if (sign.v) out.imm.v = (-1 << 12) | take_bits_range(inst.v, 20, 31);
    else        out.imm.v = ( 0 << 12) | take_bits_range(inst.v, 20, 31);
    out.write_enable.v = 1;
  }
  else if (out.opcode.v == OPCODE_SW) {
    // SW, SB
    uint32_t top_imm = take_bits_range(inst.v, 25, 31);
    uint32_t bot_imm = take_bits_range(inst.v, 7, 11);
    top_imm <<= 5;
    if (sign.v) out.imm.v = (-1 << 12) | top_imm | bot_imm;
    else        out.imm.v = ( 0 << 12) | top_imm | bot_imm;
    out.write_enable.v = 0;
  }
  else {
    out.imm.v = 0;
    out.write_enable.v = 0;
  }
  return out;
}

void print_instruction(inst_size_t inst) {
  Dec_out dec = dec_eval(inst);
  printf("[%x]: ", inst.v);
  switch (dec.opcode.v) {
    case OPCODE_ADDI: {
      printf("addi imm=%i\t rs1=r%u\t rd=r%u\n", dec.imm.v, dec.reg_src1.v, dec.reg_dest.v);
    } break;
    case OPCODE_JALR: {
      printf("jalr imm=%i\t rs1=r%u\t rd=r%u\n", dec.imm.v, dec.reg_src1.v, dec.reg_dest.v);
    } break;
    case OPCODE_ADD: { // ADD
      printf("add  rs2=r%u\t rs1=r%u\t rd=r%u\n", dec.reg_src2.v, dec.reg_src1.v, dec.reg_dest.v);
    } break;
    case OPCODE_LUI: { // LUI
      uint32_t v = sar32(dec.imm.v, 12);
      printf("lui  imm=%i\t rd=r%u\n", v, dec.reg_dest.v);
    } break;
    case OPCODE_LW: {
      if (dec.funct3.v == FUNCT3_LW) { // LW
        printf("lw   imm=%i\t rs1=r%u\t rd=r%u\n", dec.imm.v, dec.reg_src1.v, dec.reg_dest.v);
      }
      else if (dec.funct3.v == FUNCT3_LBU) { // LBU
        printf("lbu  imm=%i\t rs1=r%u\t rd=r%u\n", dec.imm.v, dec.reg_src1.v, dec.reg_dest.v);
      }
      else {
        printf("not implemented:%u\n", inst);
      }
    } break;
    case OPCODE_SW: {
      if (dec.funct3.v == FUNCT3_SW) { // SW
        printf("sw   imm=%i\t rs2=r%u\t rs1=r%u\n", dec.imm.v, dec.reg_src2.v, dec.reg_src1.v);
      }
      else if (dec.funct3.v == FUNCT3_SB) { // LBU
        printf("sb   imm=%i\t rs2=r%u\t rs1=r%u\n", dec.imm.v, dec.reg_src2.v, dec.reg_src1.v);
      }
      else {
        printf("not implemented:%u\n", inst);
      }
    } break;
    default: { // NOT IMPLEMENTED
      printf("GM WARNING: not implemented:0x%x\n", inst);
    } break;
  }
}

struct CPU_out {
  reg_index_t reg_dest;
  reg_size_t  reg_write_data;
  addr_size_t pc_addr;
  addr_size_t ram_addr;
  reg_size_t  ram_write_data;
};

CPU_out cpu_eval(miniRV* cpu, Trigger clock, Trigger reset) {
  CPU_out out = {0};
  inst_size_t inst = gm_mem_read(cpu, cpu->pc);
  Dec_out dec_out = dec_eval(inst);
  RF_out rf_out = rf_read(cpu, dec_out.reg_src1, dec_out.reg_src2);
  reg_size_t alu_out = alu_eval(dec_out.opcode, rf_out.rdata1, rf_out.rdata2, dec_out.imm);

  addr_size_t in_addr = {};
  bit is_jump = {};
  reg_size_t reg_write_data = {};

  reg_size_t ram_write_data = {};
  bit ram_write_enable = {};
  bit4 ram_write_enable_bytes = {};
  addr_size_t ram_addr = {};
  if (dec_out.opcode.v == OPCODE_ADDI) {
    // ADDI
    ram_write_enable.v = 0;
    ram_addr.v = 0;
    ram_write_data.v = 0;
    reg_write_data = alu_out;
    ram_write_enable_bytes = {0, 0, 0, 0};
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else if (dec_out.opcode.v == OPCODE_JALR) {
    // JALR
    ram_write_enable.v = 0;
    ram_addr.v = 0;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data.v= cpu->pc.v + 4;
    in_addr.v = (rf_out.rdata1.v + dec_out.imm.v) & ~3;
    is_jump.v = 1;
  }
  else if (dec_out.opcode.v == OPCODE_ADD) {
    // ADD
    ram_write_enable.v = 0;
    ram_addr.v = 0;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data = alu_out;
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else if (dec_out.opcode.v == OPCODE_LUI) {
    // LUI
    ram_write_enable.v = 0;
    ram_addr.v = 0;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data.v = dec_out.imm.v;
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else if (dec_out.opcode.v == OPCODE_LW && dec_out.funct3.v == FUNCT3_LW) {
    // LW
    ram_write_enable.v = 0;
    ram_addr.v = rf_out.rdata1.v + dec_out.imm.v;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data = gm_mem_read(cpu, ram_addr);
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else if (dec_out.opcode.v == OPCODE_LW && dec_out.funct3.v == FUNCT3_LBU) {
    // LBU
    ram_write_enable.v = 0;
    ram_addr.v = rf_out.rdata1.v + dec_out.imm.v;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data.v = gm_mem_read(cpu, ram_addr).v & 0xff;
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else if (dec_out.opcode.v == OPCODE_SW) {
    // SW
    ram_write_enable.v = 1;
    ram_addr.v = rf_out.rdata1.v + dec_out.imm.v;
    ram_write_data = rf_out.rdata2;
    if (dec_out.funct3.v == FUNCT3_SW) {
      ram_write_enable_bytes = {1, 1, 1, 1};
    }
    else if (dec_out.funct3.v == FUNCT3_SB) {
      ram_write_enable_bytes = {1, 0, 0, 0};
    }
    reg_write_data.v = 0;
    in_addr.v = 0;
    is_jump.v = 0;
  }
  else {
    print_instruction(inst);
    ram_write_enable.v = 0;
    ram_addr.v = 0;
    ram_write_data.v = 0;
    ram_write_enable_bytes = {0, 0, 0, 0};
    reg_write_data.v = 0;
    in_addr.v = 0;
    is_jump.v = 0;
  }


  out.reg_dest = dec_out.reg_dest;
  out.reg_write_data = reg_write_data;
  out.pc_addr = cpu->pc;
  out.ram_addr = ram_addr;
  out.ram_write_data = ram_write_data;

  rf_write(cpu, clock, reset, dec_out.write_enable, dec_out.reg_dest, reg_write_data);
  gm_mem_write(cpu, clock, reset, ram_write_enable, ram_write_enable_bytes, ram_addr, ram_write_data);
  pc_write(cpu, clock, reset, in_addr, is_jump);
  return out;
}

void cpu_reset(miniRV* cpu) {
  cpu->pc.v = 0;
  for (uint32_t i = 0; i < N_REGS; i++) {
    cpu->regs[i].v = 0;
  }
}

inst_size_t addi(uint32_t imm, uint32_t reg_src1, uint32_t reg_dest) {
  uint32_t inst_u32 = (imm << 20) | (reg_src1 << 15) | (FUNCT3_ADDI << 12) | (reg_dest << 7) | OPCODE_ADDI;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t li(uint32_t imm, uint32_t reg_dest) {
  return addi(imm, 0, reg_dest);
}

inst_size_t jalr(uint32_t imm, uint32_t reg_src1, uint32_t reg_dest) {
  uint32_t inst_u32 = (imm << 20) | (reg_src1 << 15) | (FUNCT3_JALR << 12) | (reg_dest << 7) | OPCODE_JALR;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t add(uint32_t reg_src2, uint32_t reg_src1, uint32_t reg_dest) {
  uint32_t inst_u32 = (0b0000000 << 25) | (reg_src2 << 20) | (reg_src1 << 15) | (FUNCT3_ADD << 12) | (reg_dest << 7) | OPCODE_ADD;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t lui(uint32_t imm, uint32_t reg_dest) {
  uint32_t inst_u32 = (imm << 12) | (reg_dest << 7) | OPCODE_LUI;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t lw(uint32_t imm, uint32_t reg_src1, uint32_t reg_dest) {
  uint32_t inst_u32 = (imm << 20) | (reg_src1 << 15) | (FUNCT3_LW << 12) | (reg_dest << 7) | OPCODE_LW;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t lbu(uint32_t imm, uint32_t reg_src1, uint32_t reg_dest) {
  uint32_t inst_u32 = (imm << 20) | (reg_src1 << 15) | (FUNCT3_LBU << 12) | (reg_dest << 7) | OPCODE_LW;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t sw(uint32_t imm, uint32_t reg_src2, uint32_t reg_src1) {
  uint32_t top_imm = (imm << 5) >> 5;
  uint32_t bot_imm = 0b11111 & imm;
  uint32_t inst_u32 = (top_imm << 25) | (reg_src2 << 20) | (reg_src1 << 15) | (FUNCT3_SW << 12) | (bot_imm << 7) | OPCODE_SW;
  inst_size_t result = { inst_u32 };
  return result;
}

inst_size_t sb(uint32_t imm, uint32_t reg_src2, uint32_t reg_src1) {
  uint32_t top_imm = imm << 5;
  uint32_t bot_imm = 0b11111 & imm;
  uint32_t inst_u32 = (top_imm << 25) | (reg_src2 << 20) | (reg_src1 << 15) | (FUNCT3_SB << 12) | (bot_imm << 7) | OPCODE_SW;
  inst_size_t result = { inst_u32 };
  return result;
}

