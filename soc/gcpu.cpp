#include <cstdint>
#include <assert.h>
#include "mem_map.h"

#define ALU_OP_ADD  (0b0000)
#define ALU_OP_SUB  (0b1000) 
#define ALU_OP_SLL  (0b0001) 
#define ALU_OP_SLT  (0b0010) 
#define ALU_OP_SLTU (0b0011) 
#define ALU_OP_XOR  (0b0100) 
#define ALU_OP_SRL  (0b0101) 
#define ALU_OP_SRA  (0b1101) 
#define ALU_OP_OR   (0b0110) 
#define ALU_OP_AND  (0b0111) 

#define COM_OP_EQ  (0b000)
#define COM_OP_NE  (0b001)
#define COM_OP_LT  (0b100)
#define COM_OP_GE  (0b101)
#define COM_OP_LTU (0b110)
#define COM_OP_GEU (0b111)

#define INST_UNDEFINED  (0b00000)

#define INST_EBREAK     (0b10000)
#define INST_CSRRW      (0b10001)
#define INST_CSRRS      (0b10010)
#define INST_CSRRC      (0b10011)
#define INST_CSRRWI     (0b10101)
#define INST_CSRRSI     (0b10110)
#define INST_CSRRCI     (0b10111)

#define INST_LOAD_BYTE  (0b01100)
#define INST_LOAD_HALF  (0b01101)
#define INST_LOAD_WORD  (0b01110)
#define INST_STORE      (0b01000)

#define INST_BRANCH     (0b00011)
#define INST_IMM        (0b00100)
#define INST_REG        (0b00101)
#define INST_UPP        (0b00110)
#define INST_JUMP       (0b00111)
#define INST_JUMPR      (0b01111)
#define INST_AUIPC      (0b11111)

enum VerboseLevel {
  VerboseNone,
  VerboseError,
  VerboseFailed,
  VerboseWarning,
  VerboseInfo4,
  VerboseInfo5,
  VerboseInfo6,
};

struct Vuart {
  uint8_t&  ier;
  uint8_t&  iir;
  uint8_t&  fcr;
  uint8_t&  mcr;
  uint8_t&  msr;
  uint8_t&  lcr;
  uint8_t&  lsr;
  uint8_t&  lsr0;
  uint8_t&  lsr1;
  uint8_t&  lsr2;
  uint8_t&  lsr3;
  uint8_t&  lsr4;
  uint8_t&  lsr5;
  uint8_t&  lsr6;
  uint8_t&  lsr7;

  bool      lsr_packed;
};

struct VSoCcpu {
  uint8_t & ebreak;
  uint32_t& pc;
  uint64_t& mcycle;
  uint64_t& minstret;
  VlUnpacked<uint32_t, 16>&  regs;
  VlUnpacked<uint16_t, 16777216>& mem;
  uint64_t minstret_start;
  Vuart uart;
};

struct Gcpu {
  uint32_t pc = INITIAL_PC;
  uint32_t regs[N_REGS];

  uint8_t mem[MEM_SIZE+4];
  uint8_t flash[FLASH_SIZE+4];

  uint8_t ebreak           = false;
  bool    is_not_mapped    = false;
  bool    is_mem_write     = false;
  uint32_t written_address = 0;
  VerboseLevel verbose     = VerboseFailed;
  Vuart*  vuart;
};

void g_reset(Gcpu* cpu) {
  printf("[INFO] gold reset\n");
  // memset(cpu->mem, 0, MEM_SIZE);
  // memset(cpu->flash, 0, FLASH_SIZE);
  cpu->pc = INITIAL_PC;
  cpu->ebreak = 0;
  for (uint32_t i = 0; i < N_REGS; i++) {
    cpu->regs[i] = 0;
  }
  cpu->is_not_mapped = 0;
}

void g_flash_init(Gcpu* cpu, uint8_t* data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    cpu->flash[i] = data[i];
  }
  printf("[INFO] gold flash written: %u bytes\n", size);
}

void g_mem_write(Gcpu* cpu, uint8_t wen, uint8_t wbmask, uint32_t addr, uint32_t wdata) {
  cpu->is_mem_write = wen;
  if (wen) {
    cpu->written_address = addr;
    if (addr >= FLASH_START && addr < FLASH_END-3) {
      cpu->is_not_mapped = true;
      if (cpu->verbose >= VerboseWarning) {
        printf("[WARNING]: gcpu tried to write to flash memory 0x%x\n", addr);
      }
    }
    else if (addr >= UART_START && addr < UART_END) {
    }
    else if (addr >= MEM_START && addr < MEM_END-3) {
      addr -= MEM_START;
      if (wbmask & 0b0001) cpu->mem[addr + 0] = (wdata >>  0) & 0xff;
      if (wbmask & 0b0010) cpu->mem[addr + 1] = (wdata >>  8) & 0xff;
      if (wbmask & 0b0100) cpu->mem[addr + 2] = (wdata >> 16) & 0xff;
      if (wbmask & 0b1000) cpu->mem[addr + 3] = (wdata >> 24) & 0xff;
    }
    else {
      cpu->is_not_mapped = true;
      if (cpu->verbose >= VerboseWarning) {
        printf("[WARNING]: gcpu mem write memory is not mapped 0x%x\n", addr);
      }
    }
  }
}

uint32_t g_mem_read(Gcpu* cpu, uint32_t addr) {
  uint32_t result = 0;
  if (addr >= FLASH_START && addr < FLASH_END-3) {
    if (addr & 3) {
      cpu->is_not_mapped = true;
      if (cpu->verbose >= VerboseWarning) {
        printf("[WARNING]: gcpu misaligned flash read 0x%x\n", addr);
      }
    }

    addr -= FLASH_START;
    result = 
      cpu->flash[addr+3] << 24 | cpu->flash[addr+2] << 16 |
      cpu->flash[addr+1] <<  8 | cpu->flash[addr+0] <<  0 ;
  }
  else if (addr >= UART_START && addr < UART_END) {
    addr -= UART_START;
    uint8_t byte = 0;
    switch (addr) {
      case 1 : byte = cpu->vuart->ier; break;
      case 2 : byte = cpu->vuart->iir; break;
      case 3 : byte = cpu->vuart->lcr; break;
      case 5 : {
        if (cpu->vuart->lsr_packed) byte = cpu->vuart->lsr;
        else byte =
          (cpu->vuart->lsr0 << 0) |
          (cpu->vuart->lsr1 << 1) |
          (cpu->vuart->lsr2 << 2) |
          (cpu->vuart->lsr3 << 3) |
          (cpu->vuart->lsr4 << 4) |
          (cpu->vuart->lsr5 << 5) |
          (cpu->vuart->lsr6 << 6) |
          (cpu->vuart->lsr7 << 7) ;
      } break;
      case 6 : byte = cpu->vuart->msr; break;
      default:
        cpu->is_not_mapped = true;
        if (cpu->verbose >= VerboseWarning) {
          printf("[WARNING]: gcpu uart register is not implemented 0x%x\n", addr);
        }
        break;
    }
    result = 
      byte << 24 | byte << 16 |
      byte <<  8 | byte <<  0 ;
  }
  else if (addr >= MEM_START && addr < MEM_END-3) {
    addr -= MEM_START;
    result = 
      cpu->mem[addr+3] << 24 | cpu->mem[addr+2] << 16 |
      cpu->mem[addr+1] <<  8 | cpu->mem[addr+0] <<  0 ;
  }
  else {
    cpu->is_not_mapped = true;
    if (cpu->verbose >= VerboseWarning) {
      printf("[WARNING]: gcpu mem read  memory is not mapped 0x%x\n", addr);
    }
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
    case ALU_OP_ADD:  result = lhs + rhs;         break;
    case ALU_OP_SUB:  result = lhs - rhs;         break;
    case ALU_OP_XOR:  result = lhs ^ rhs;         break;
    case ALU_OP_AND:  result = lhs & rhs;         break;
    case ALU_OP_OR:   result = lhs | rhs;         break;
    case ALU_OP_SLL:  result = lhs << shamt;      break;
    case ALU_OP_SRL:  result = lhs >> shamt;      break;
    case ALU_OP_SRA:  result = sra32(lhs, shamt); break;
    case ALU_OP_SLT:  result = slt(lhs, rhs);     break;
    case ALU_OP_SLTU: result = lhs < rhs;         break;
  }
  return result;
}

uint32_t compare(uint8_t op, uint32_t lhs, uint32_t rhs) {
  uint32_t result = 0;
  switch (op) {
    case COM_OP_EQ:  result = lhs == rhs;      break;
    case COM_OP_NE:  result = lhs != rhs;      break;
    case COM_OP_LT:  result = slt(lhs, rhs);   break;
    case COM_OP_GE:  result = !slt(lhs, rhs);  break;
    case COM_OP_LTU: result = lhs <  rhs;      break;
    case COM_OP_GEU: result = lhs >= rhs;      break;
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
  uint8_t  reg_dest;
  uint8_t  reg_src1;
  uint8_t  reg_src2;
  uint32_t imm;
  uint8_t  mem_wbmask;
  uint8_t  is_mem_sign;
  uint8_t  alu_op;
  uint8_t  com_op;
  uint8_t  ebreak;
  uint32_t inst_type;
  bool     not_implemented;
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

  uint32_t j_imm = 0;
  if (sign) j_imm = (~0u << 20) | take_bits_range(inst, 12, 19) << 12 | take_bit(inst, 20) << 11 | take_bits_range(inst, 21, 30) << 1 | 0b0;
  else      j_imm = ( 0u << 20) | take_bits_range(inst, 12, 19) << 12 | take_bit(inst, 20) << 11 | take_bits_range(inst, 21, 30) << 1 | 0b0;

  uint32_t b_imm = 0;
  if (sign) b_imm = (~0u << 12) | take_bit(inst, 7) << 11 | take_bits_range(inst, 25, 30) << 5 | take_bits_range(inst, 8, 11) << 1 | 0b0;
  else      b_imm = ( 0u << 12) | take_bit(inst, 7) << 11 | take_bits_range(inst, 25, 30) << 5 | take_bits_range(inst, 8, 11) << 1 | 0b0;

  out.alu_op = ALU_OP_ADD;
  switch (opcode) {
    case OPCODE_CALC_IMM: {
      out.imm = i_imm;
      out.inst_type = INST_IMM;
      out.alu_op = (sub & funct3==FUNCT3_SR) << 3 | funct3;
    } break;
    case OPCODE_CALC_REG: {
      out.inst_type = INST_REG;
      out.alu_op = (sub & (funct3==FUNCT3_SR || funct3==FUNCT3_ADD)) << 3 | funct3;
    } break;
    case OPCODE_LOAD: {
      out.imm = i_imm;
      switch (funct3) {
        case FUNCT3_LB:  out.inst_type = (0b011 << 2)|funct3 & 0b11; break;
        case FUNCT3_LH:  out.inst_type = (0b011 << 2)|funct3 & 0b11; break;
        case FUNCT3_LW:  out.inst_type = (0b011 << 2)|funct3 & 0b11; break;
        case FUNCT3_LBU: out.inst_type = (0b011 << 2)|funct3 & 0b11; break;
        case FUNCT3_LHU: out.inst_type = (0b011 << 2)|funct3 & 0b11; break;
        default:         out.inst_type = 0;                          break;
      }
    } break;
    case OPCODE_STORE: {
      out.imm = s_imm;
      switch (funct3) {
        case FUNCT3_SB: out.mem_wbmask = 0b0001; break;
        case FUNCT3_SH: out.mem_wbmask = 0b0011; break;
        case FUNCT3_SW: out.mem_wbmask = 0b1111; break;
        default:        out.mem_wbmask = 0b0000; break;
      }
      out.inst_type = INST_STORE;
    } break;
    case OPCODE_LUI: {
      out.imm = u_imm;
      out.inst_type = INST_UPP;
    } break;
    case OPCODE_AUIPC: {
      out.imm = u_imm;
      out.inst_type = INST_AUIPC;
    } break;
    case OPCODE_JALR: {
      out.imm = i_imm;
      out.inst_type = INST_JUMPR;
    } break;
    case OPCODE_JAL: {
      out.imm = j_imm;
      out.inst_type = INST_JUMP;
    } break;
    case OPCODE_BRANCH: {
      out.imm = b_imm;
      out.inst_type = INST_BRANCH;
      out.com_op = funct3;
    } break;
    case OPCODE_SYSTEM: {
      out.inst_type = 0;
      out.ebreak = take_bit(inst, 20);
    } break;
    default:
      out.inst_type = 0;
      break;
  }

  out.is_mem_sign = !(funct3 & 0b100);
  return out;
}

uint8_t cpu_eval(Gcpu* cpu) {
  uint32_t inst = g_mem_read(cpu, cpu->pc);
  Dec_out  dec  = decode(inst);
  if (dec.inst_type == 0) cpu->is_not_mapped = 1;
  RF_out   rf   = rf_read(cpu, dec.reg_src1, dec.reg_src2);
  bool is_mem_op =
    dec.inst_type == INST_LOAD_BYTE ||
    dec.inst_type == INST_LOAD_HALF ||
    dec.inst_type == INST_LOAD_WORD ||
    dec.inst_type == INST_STORE ;

  uint32_t alu_lhs = 0;
  switch (dec.inst_type) {
    case INST_JUMP:      alu_lhs = cpu->pc;   break; 
    case INST_AUIPC:     alu_lhs = cpu->pc;   break; 
    case INST_BRANCH:    alu_lhs = cpu->pc;   break; 
    default:             alu_lhs = rf.rdata1; break;
  }
  uint32_t alu_rhs = 0;
  switch (dec.inst_type) {
    case INST_REG:       alu_rhs = rf.rdata2; break; 
    case INST_UPP:       alu_rhs = 0;         break;
    default:             alu_rhs = dec.imm;   break;
  }

  uint32_t alu_res = alu_eval(dec.alu_op, alu_lhs, alu_rhs);
  uint32_t com_res =  compare(dec.com_op, rf.rdata1, rf.rdata2);

  uint32_t mem_rdata = is_mem_op ? g_mem_read(cpu, alu_res) : 0;
  uint32_t mem_rdata_byte = take_bits_range(mem_rdata, 0, 7);
  uint32_t mem_rdata_half = take_bits_range(mem_rdata, 0, 15);
  uint32_t mem_rdata_byte_sign = take_bit(mem_rdata, 7)  && dec.is_mem_sign;
  uint32_t mem_rdata_half_sign = take_bit(mem_rdata, 15) && dec.is_mem_sign;
  uint32_t mem_byte_extend = mem_rdata_byte_sign ? (~0 <<  8) | mem_rdata_byte : mem_rdata_byte;
  uint32_t mem_half_extend = mem_rdata_half_sign ? (~0 << 16) | mem_rdata_half : mem_rdata_half;

  uint8_t pc_jump   = 0; uint8_t mem_wen   = 0; uint8_t reg_wen   = 0; uint32_t reg_wdata = 0;
  switch (dec.inst_type) {
    case INST_LOAD_BYTE: pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = mem_byte_extend; break;
    case INST_LOAD_HALF: pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = mem_half_extend; break;
    case INST_LOAD_WORD: pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = mem_rdata;       break;
    case INST_UPP:       pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = dec.imm;         break;
    case INST_JUMP:      pc_jump = 1;       mem_wen = 0; reg_wen = 1; reg_wdata = cpu->pc+4;       break;
    case INST_JUMPR:     pc_jump = 1;       mem_wen = 0; reg_wen = 1; reg_wdata = cpu->pc+4;       break;
    case INST_AUIPC:     pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = alu_res;         break;
    case INST_REG:       pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = alu_res;         break;
    case INST_IMM:       pc_jump = 0;       mem_wen = 0; reg_wen = 1; reg_wdata = alu_res;         break;
    case INST_STORE:     pc_jump = 0;       mem_wen = 1; reg_wen = 0; reg_wdata = 0;               break;
    case INST_BRANCH:    pc_jump = com_res; mem_wen = 0; reg_wen = 0; reg_wdata = 0;               break;
    default:             pc_jump = 0;       mem_wen = 0; reg_wen = 0; reg_wdata = 0;               break;
  }

  rf_write(cpu, reg_wen, dec.reg_dest, reg_wdata);
  g_mem_write(cpu, mem_wen, dec.mem_wbmask, alu_res, rf.rdata2);
  pc_write(cpu, alu_res, pc_jump);
  cpu->ebreak = dec.ebreak;
  return dec.ebreak;
}
