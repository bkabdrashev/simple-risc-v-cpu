#include <cstdint>
#include <assert.h>
#include "c_dpi.h"
#include "mem_map.h"

struct Gcpu {
  uint32_t pc = INITIAL_PC;
  uint32_t regs[N_REGS];

  uint8_t mem[MEM_SIZE];
  uint8_t flash[FLASH_SIZE];

  uint8_t ebreak;

  uint8_t*  uart_status_ref;
  uint64_t* time_uptime_ref;
};

void g_reset(Gcpu* cpu) {
  memset(cpu->mem, 0, MEM_SIZE);
  memset(cpu->flash, 0, FLASH_SIZE);
  cpu->pc = INITIAL_PC;
  cpu->ebreak = 0;
  for (uint32_t i = 0; i < N_REGS; i++) {
    cpu->regs[i] = 0;
  }
}

void g_flash_init(Gcpu* cpu, uint8_t* data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    cpu->flash[i] = data[i];
  }
  printf("[INFO] g flash written: %u bytes\n", size);
}

void g_mem_write(Gcpu* cpu, uint8_t wen, uint8_t wbmask, uint32_t addr, uint32_t wdata) {
  if (wen) {
    if (addr >= FLASH_START && addr < FLASH_END-3) {
      // addr -= FLASH_START;
      // if (wbmask & 0b0001) cpu->flash[addr + 0] = (wdata & (0xff <<  0)) >> 0;
      // if (wbmask & 0b0010) cpu->flash[addr + 1] = (wdata & (0xff <<  8)) >> 8;
      // if (wbmask & 0b0100) cpu->flash[addr + 2] = (wdata & (0xff << 16)) >> 16;
      // if (wbmask & 0b1000) cpu->flash[addr + 3] = (wdata & (0xff << 24)) >> 23;
    }
    else if (addr >= MEM_START && addr < MEM_END-3) {
      addr -= MEM_START;
      if (wbmask & 0b0001) cpu->mem[addr + 0] = (wdata & (0xff <<  0)) >> 0;
      if (wbmask & 0b0010) cpu->mem[addr + 1] = (wdata & (0xff <<  8)) >> 8;
      if (wbmask & 0b0100) cpu->mem[addr + 2] = (wdata & (0xff << 16)) >> 16;
      if (wbmask & 0b1000) cpu->mem[addr + 3] = (wdata & (0xff << 24)) >> 23;
    }
    // else if (addr.v == UART_DATA_ADDR) {
    //   putc(write_data.v & 0xff, stderr);
    // }
    else {
      // printf("GM WARNING: mem write memory is not mapped\n");
    }
  }
}

uint32_t g_mem_read(Gcpu* cpu, uint32_t addr) {
  uint32_t result = 0;
  if (addr >= FLASH_START && addr < FLASH_END-3) {
    addr -= FLASH_START;
    result = 
      cpu->flash[addr+3] << 24 | cpu->flash[addr+2] << 16 |
      cpu->flash[addr+1] <<  8 | cpu->flash[addr+0] <<  0 ;
  }
  else if (addr >= MEM_START && addr < MEM_END-3) {
    addr -= MEM_START;
    result = 
      cpu->mem[addr+3] << 24 | cpu->mem[addr+2] << 16 |
      cpu->mem[addr+1] <<  8 | cpu->mem[addr+0] <<  0 ;
  }
  else {
    // printf("GM WARNING: mem read memory is not mapped\n");
  }
  return result;
}

static int32_t u32_as_i32(uint32_t u) {
  int32_t s;
  static_assert(sizeof(s) == sizeof(u));
  memcpy(&s, &u, sizeof(s));
  return s;
}

static bool slt(uint32_t a, uint32_t b) {
  return u32_as_i32(a) < u32_as_i32(b);
}

uint32_t alu_eval(uint8_t op, uint32_t lhs, uint32_t rhs) {
  uint32_t shamt = take_bits_range(rhs, 0, 4);
  uint32_t result = 0;
  switch (op) {
    case OP_ADD:  result = lhs + rhs; break;
    case OP_SUB:  result = lhs - rhs; break;
    case OP_XOR:  result = lhs ^ rhs; break;
    case OP_AND:  result = lhs & rhs; break;
    case OP_OR:   result = lhs | rhs; break;
    case OP_SLL:  result = lhs << shamt; break;
    case OP_SRL:  result = lhs >> shamt; break;
    case OP_SRA:  result = sar32(lhs, shamt); break;
    case OP_SLT:  result = slt(lhs, rhs); break;
    case OP_SLTU: result = lhs < rhs; break;
  }
  return result;
}

void pc_write(Gcpu* cpu, uint32_t in_addr, uint8_t is_jump) {
  if (is_jump) {
    cpu->pc = in_addr;
  }
  else {
    cpu->pc += 4;
  }
}

struct RF_out {
  uint32_t rdata1;
  uint32_t rdata2;
};

RF_out rf_read(Gcpu* cpu, uint8_t reg_src1, uint8_t reg_src2) {
  RF_out out = {};
  if (reg_src1 < N_REGS) out.rdata1 = cpu->regs[reg_src1];
  if (reg_src2 < N_REGS) out.rdata2 = cpu->regs[reg_src2];
  return out;
}

void rf_write(Gcpu* cpu, uint8_t write_enable, uint8_t reg_dest, uint32_t write_data) {
  if (write_enable && reg_dest != 0 && reg_dest < N_REGS) {
    cpu->regs[reg_dest] = write_data;
  }
}

struct Dec_out {
  uint8_t reg_dest;
  uint8_t reg_src1;
  uint8_t reg_src2;
  uint32_t imm;
  uint8_t mem_wbmask;
  uint8_t is_mem_sign;
  uint8_t alu_op;
  uint8_t ebreak;
  uint32_t inst_type;
};

Dec_out decode(uint32_t inst) {
  Dec_out out = {};
  uint8_t opcode = take_bits_range(inst, 0, 6);
  out.reg_dest   = take_bits_range(inst, 7, 11);
  uint8_t funct3 = take_bits_range(inst, 12, 14);
  uint8_t funct7 = take_bits_range(inst, 25, 31);
  out.reg_src1   = take_bits_range(inst, 15, 19);
  out.reg_src2   = take_bits_range(inst, 20, 24);
  uint8_t sign   = take_bit(inst, 31);
  uint8_t sub    = take_bit(inst, 30);


  uint32_t i_imm = 0;
  if (sign) i_imm = (~0u << 12) | take_bits_range(inst, 20, 31);
  else      i_imm = ( 0u << 12) | take_bits_range(inst, 20, 31);

  uint32_t u_imm = take_bits_range(inst, 12, 31) << 12;

  uint32_t s_imm   = 0;
  uint32_t top_imm = take_bits_range(inst, 25, 31) << 5;
  uint32_t bot_imm = take_bits_range(inst, 7, 11);
  if (sign) s_imm = (~0u << 12) | top_imm | bot_imm;
  else      s_imm = ( 0u << 12) | top_imm | bot_imm;

  switch (opcode) {
    case OPCODE_CALC_IMM: {
      out.imm = i_imm;
      out.inst_type = INST_IMM;
      out.alu_op = (sub & funct3==FUNCT3_SR) << 3 | funct3;
    } break;
    case OPCODE_CALC_REG: {
      out.inst_type = INST_REG;
      out.alu_op = sub << 3 | funct3;
    } break;
    case OPCODE_LOAD: {
      out.imm = i_imm;
      switch (funct3) {
        case FUNCT3_BYTE:        out.inst_type = (0b10 << 2)|funct3 & 0b11; break;
        case FUNCT3_HALF:        out.inst_type = (0b10 << 2)|funct3 & 0b11; break;
        case FUNCT3_WORD:        out.inst_type = (0b10 << 2)|funct3 & 0b11; break;
        case FUNCT3_BYTE_UNSIGN: out.inst_type = (0b10 << 2)|funct3 & 0b11; break;
        case FUNCT3_HALF_UNSIGN: out.inst_type = (0b10 << 2)|funct3 & 0b11; break;
        default:                 out.inst_type = 0;                         break;
      }
    } break;
    case OPCODE_STORE: {
      out.imm = s_imm;
      switch (funct3) {
        case FUNCT3_BYTE: out.mem_wbmask = 0b0001; break;
        case FUNCT3_HALF: out.mem_wbmask = 0b0011; break;
        case FUNCT3_WORD: out.mem_wbmask = 0b1111; break;
        default:          out.mem_wbmask = 0b0000; break;
      }
      out.inst_type = INST_STORE;
    } break;
    case OPCODE_LUI: {
      out.imm = u_imm;
      out.inst_type = INST_UPP;
    } break;
    case OPCODE_JALR: {
      out.imm = i_imm;
      out.inst_type = INST_JUMP;
    } break;
    case OPCODE_ENV: {
      out.inst_type = 0;
      out.ebreak = take_bit(inst, 20);
    } break;
    default: out.inst_type = 0; break;
  }

  out.is_mem_sign = !(funct3 & 0b100);
  return out;
}

uint8_t cpu_eval(Gcpu* cpu) {
  uint32_t inst = g_mem_read(cpu, cpu->pc);
  Dec_out  dec  = decode(inst);
  RF_out   rf   = rf_read(cpu, dec.reg_src1, dec.reg_src2);

  uint32_t alu_rhs = 0;
  switch (dec.inst_type) {
    case INST_REG:       alu_rhs = rf.rdata2; break; 
    case INST_LOAD_BYTE: alu_rhs = dec.imm;       break;
    case INST_LOAD_HALF: alu_rhs = dec.imm;       break;
    case INST_LOAD_WORD: alu_rhs = dec.imm;       break;
    case INST_STORE:     alu_rhs = dec.imm;       break;
    case INST_JUMP:      alu_rhs = dec.imm;       break; 
    case INST_IMM:       alu_rhs = dec.imm;       break;
    case INST_UPP:       alu_rhs = 0;             break;
    default:             alu_rhs = 0;             break;
  }
  uint32_t alu_res = alu_eval(dec.alu_op, rf.rdata1, alu_rhs);

  uint32_t mem_rdata = g_mem_read(cpu, alu_res);
  uint32_t mem_rdata_byte = take_bits_range(mem_rdata, 0, 7);
  uint32_t mem_rdata_half = take_bits_range(mem_rdata, 0, 15);
  uint32_t mem_rdata_byte_sign = take_bit(mem_rdata, 7)  && dec.is_mem_sign;
  uint32_t mem_rdata_half_sign = take_bit(mem_rdata, 15) && dec.is_mem_sign;
  uint32_t mem_byte_extend = mem_rdata_byte_sign ? (~0 <<  8) | mem_rdata_byte : mem_rdata_byte;
  uint32_t mem_half_extend = mem_rdata_half_sign ? (~0 << 16) | mem_rdata_half : mem_rdata_half;

  uint32_t reg_wdata = 0;
  uint8_t reg_wen   = 0;
  uint8_t mem_wen   = 0;
  uint8_t pc_jump   = 0;
  switch (dec.inst_type) {
    case INST_LOAD_BYTE: pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = mem_byte_extend; break;
    case INST_LOAD_HALF: pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = mem_half_extend; break;
    case INST_LOAD_WORD: pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = mem_rdata;       break;
    case INST_UPP:       pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = dec.imm;         break;
    case INST_JUMP:      pc_jump = 1; mem_wen = 0; reg_wen = 1; reg_wdata = cpu->pc+4;       break;
    case INST_REG:       pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = alu_res;         break;
    case INST_IMM:       pc_jump = 0; mem_wen = 0; reg_wen = 1; reg_wdata = alu_res;         break;
    case INST_STORE:     pc_jump = 0; mem_wen = 1; reg_wen = 1; reg_wdata = 0;               break;
    default:             pc_jump = 0; mem_wen = 0; reg_wen = 0; reg_wdata = 0;               break;
  }

  rf_write(cpu, reg_wen, dec.reg_dest, reg_wdata);
  g_mem_write(cpu, mem_wen, dec.mem_wbmask, alu_res, rf.rdata2);
  pc_write(cpu, alu_res, pc_jump);
  cpu->ebreak = dec.ebreak;
  return dec.ebreak;
}
