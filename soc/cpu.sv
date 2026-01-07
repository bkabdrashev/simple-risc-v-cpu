module cpu (
  input         clock,
  input         reset,

  input  [31:0] io_ifu_rdata,
  input         io_ifu_respValid,
  output        io_ifu_reqValid,
  output [31:0] io_ifu_addr,

  input         io_lsu_respValid,
  input  [31:0] io_lsu_rdata,
  output        io_lsu_reqValid,
  output [31:0] io_lsu_addr,
  output [1:0]  io_lsu_size,
  output        io_lsu_wen,
  output [31:0] io_lsu_wdata,
  output [3:0]  io_lsu_wmask);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic                  ebreak;
  logic                  is_done_instruction;

  logic [REG_END_ID:0]   rd;
  logic [REG_END_ID:0]   rs1;
  logic [REG_END_ID:0]   rs2;
  logic [REG_END_WORD:0] imm;

  logic [REG_END_WORD:0] pc;
  logic                  pc_wen;
  logic                  is_pc_jump;
  logic [REG_END_WORD:0] pc_next;
  logic [REG_END_WORD:0] pc_inc;

  logic [REG_END_WORD:0] alu_res;
  logic [REG_END_WORD:0] alu_lhs;
  logic [REG_END_WORD:0] alu_rhs;
  logic [3:0]            alu_op;

  logic [2:0]            com_op;
  logic                  com_res;

  logic [REG_END_WORD:0] reg_wdata;
  logic [REG_END_WORD:0] reg_rdata1;
  logic [REG_END_WORD:0] reg_rdata2;

  logic is_mem_sign;

  logic mem_byte_sign;
  logic mem_half_sign;

  logic [REG_END_WORD-REG_END_BYTE-1:0] mem_byte_extend;
  logic [REG_END_WORD-REG_END_HALF-1:0] mem_half_extend;

  logic [INST_TYPE_END:0]  inst_type;

  logic reg_wen;
  logic [7:0]  byte2;
  logic [15:0] half2;
  logic [3:0] lsu_wmask;
  logic [31:0] lsu_wdata;
  logic [31:0] lsu_rdata;
  logic [31:0] lsu_addr;
  logic [1:0]  lsu_size;

  logic [REG_END_WORD:0] csr_rdata;
  logic [REG_END_WORD:0] csr_wdata;
  logic        csr_wen;

  pc u_pc(
    .clock(clock),
    .reset(reset),
    .in_addr(pc_next),
    .out_addr(pc));

  dec u_dec(
    .inst(io_ifu_rdata),
    .clock(clock),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .is_mem_sign(is_mem_sign),
    .alu_op(alu_op),
    .com_op(com_op),
    .imm(imm),
    .inst_type(inst_type));

  alu u_alu(
    .op(alu_op),
    .lhs(alu_lhs),
    .rhs(alu_rhs),
    .res(alu_res));

  com u_com(
    .op(com_op),
    .lhs(reg_rdata1),
    .rhs(reg_rdata2),
    .res(com_res));

  rf u_rf(
    .clock(clock),
    .reset(reset),

    .wen(reg_wen),
    .wdata(reg_wdata),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .rdata1(reg_rdata1),
    .rdata2(reg_rdata2));

  csr u_csr(
    .clock(clock),
    .reset(reset),
    .wen(csr_wen),
    .addr(imm[11:0]),
    .is_done_inst(is_done_instruction),
    .wdata(csr_wdata),
    .rdata(csr_rdata));

  sm u_sm(
    .clock(clock),
    .reset(reset),

    .ifu_respValid(io_ifu_respValid),
    .lsu_respValid(io_lsu_respValid),
    .inst_type(inst_type),

    .reg_wen(reg_wen),
    .pc_wen(pc_wen),
    .lsu_wen(io_lsu_wen),

    .ifu_reqValid(io_ifu_reqValid),
    .lsu_reqValid(io_lsu_reqValid),
    .finished(is_done_instruction),
    .ebreak(ebreak));

  always_comb begin
    pc_inc = pc + 4;

    case (inst_type)
      INST_CSRRC:  alu_lhs = ~reg_rdata1;
      INST_CSRRWI: alu_lhs = {27'b0,  rs1};
      INST_CSRRSI: alu_lhs = {27'b0,  rs1};
      INST_CSRRCI: alu_lhs = {27'b1, ~rs1};
      INST_JUMP:   alu_lhs = pc;
      INST_AUIPC:  alu_lhs = pc;
      INST_BRANCH: alu_lhs = pc;
      default:     alu_lhs = reg_rdata1;
    endcase

    case (inst_type)
      INST_REG:        alu_rhs = reg_rdata2;        
      INST_LOAD_BYTE:  alu_rhs = imm;
      INST_LOAD_HALF:  alu_rhs = imm;
      INST_LOAD_WORD:  alu_rhs = imm;
      INST_STORE_BYTE: alu_rhs = imm;
      INST_STORE_HALF: alu_rhs = imm;
      INST_STORE_WORD: alu_rhs = imm;
      INST_JUMP:       alu_rhs = imm;
      INST_JUMPR:      alu_rhs = imm;
      INST_AUIPC:      alu_rhs = imm;
      INST_BRANCH:     alu_rhs = imm;
      INST_IMM:        alu_rhs = imm;        
      INST_UPP:        alu_rhs = 0;        

      INST_CSRRW:      alu_rhs = 0;
      INST_CSRRS:      alu_rhs = csr_rdata;
      INST_CSRRC:      alu_rhs = csr_rdata;
      INST_CSRRWI:     alu_rhs = 0;
      INST_CSRRSI:     alu_rhs = csr_rdata;
      INST_CSRRCI:     alu_rhs = csr_rdata;

      default:         alu_rhs = 0;
    endcase

    case (inst_type)
      INST_CSRRW:      csr_wen = 1;
      INST_CSRRS:      csr_wen = 1;
      INST_CSRRC:      csr_wen = 1;
      INST_CSRRWI:     csr_wen = 1;
      INST_CSRRSI:     csr_wen = 1;
      INST_CSRRCI:     csr_wen = 1;
      default:         csr_wen = 0;
    endcase

    byte2 = reg_rdata2[7:0];
    half2 = reg_rdata2[15:0];

    lsu_size  = 0;
    lsu_wdata = 0;
    lsu_rdata = 0;
    lsu_wmask = 0;
    lsu_addr = alu_res;

    csr_wdata = alu_res;

    case (inst_type)
      INST_STORE_BYTE: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b0001; lsu_wdata = {8'b0, 8'b0, 8'b0, byte2}; end
          2'b01: begin lsu_wmask = 4'b0010; lsu_wdata = {8'b0, 8'b0, byte2, 8'b0}; end
          2'b10: begin lsu_wmask = 4'b0100; lsu_wdata = {8'b0, byte2, 8'b0, 8'b0}; end
          2'b11: begin lsu_wmask = 4'b1000; lsu_wdata = {byte2, 8'b0, 8'b0, 8'b0}; end
        endcase
      end
      INST_STORE_HALF: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b0011; lsu_wdata = {8'b0, 8'b0, half2}; end
          2'b01: begin lsu_wmask = 4'b0110; lsu_wdata = {8'b0, half2, 8'b0}; end
          2'b10: begin lsu_wmask = 4'b1100; lsu_wdata = {half2, 8'b0, 8'b0}; end
          2'b11: begin lsu_wmask = 4'b1000; lsu_wdata = {half2 << 8, 8'b0, 8'b0}; end // TODO: exceptional case, do it in 2 cycles
        endcase
      end
      INST_STORE_WORD: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b1111; lsu_wdata = reg_rdata2 <<  0; end
          2'b01: begin lsu_wmask = 4'b1110; lsu_wdata = reg_rdata2 <<  8; end // TODO: exceptional case, do it in 2 cycles
          2'b10: begin lsu_wmask = 4'b1100; lsu_wdata = reg_rdata2 << 16; end // TODO: exceptional case, do it in 2 cycles
          2'b11: begin lsu_wmask = 4'b1000; lsu_wdata = reg_rdata2 << 24; end // TODO: exceptional case, do it in 2 cycles
        endcase
      end
      INST_LOAD_BYTE: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = {24'b0, io_lsu_rdata[ 7: 0]}; end
          2'b01: begin lsu_rdata = {24'b0, io_lsu_rdata[15: 8]}; end
          2'b10: begin lsu_rdata = {24'b0, io_lsu_rdata[23:16]}; end
          2'b11: begin lsu_rdata = {24'b0, io_lsu_rdata[31:24]}; end
        endcase
      end
      INST_LOAD_HALF: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = {16'b0, io_lsu_rdata[15: 0]}; end
          2'b01: begin lsu_rdata = {16'b0, io_lsu_rdata[23: 8]}; end
          2'b10: begin lsu_rdata = {16'b0, io_lsu_rdata[31:16]}; end
          2'b11: begin lsu_rdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      INST_LOAD_WORD: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = io_lsu_rdata; end
          2'b01: begin lsu_rdata = 32'b0; end // TODO: exceptional case
          2'b10: begin lsu_rdata = 32'b0; end // TODO: exceptional case
          2'b11: begin lsu_rdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      default:  begin end
    endcase
    
    mem_byte_sign   = lsu_rdata[REG_END_BYTE] & is_mem_sign;
    mem_half_sign   = lsu_rdata[REG_END_HALF] & is_mem_sign;
    mem_byte_extend = {(REG_END_WORD-REG_END_BYTE){mem_byte_sign}};
    mem_half_extend = {(REG_END_WORD-REG_END_HALF){mem_half_sign}};
    case (inst_type)
      INST_LOAD_BYTE: reg_wdata = {mem_byte_extend, lsu_rdata[REG_END_BYTE:0]};
      INST_LOAD_HALF: reg_wdata = {mem_half_extend, lsu_rdata[REG_END_HALF:0]};
      INST_LOAD_WORD: reg_wdata = lsu_rdata;
      INST_UPP:       reg_wdata = imm;
      INST_JUMP:      reg_wdata = pc_inc;
      INST_JUMPR:     reg_wdata = pc_inc;
      INST_AUIPC:     reg_wdata = alu_res;
      INST_REG:       reg_wdata = alu_res;        
      INST_IMM:       reg_wdata = alu_res;        

      INST_CSRRW:     reg_wdata = csr_rdata;
      INST_CSRRS:     reg_wdata = csr_rdata;
      INST_CSRRC:     reg_wdata = csr_rdata;
      INST_CSRRWI:    reg_wdata = csr_rdata;
      INST_CSRRSI:    reg_wdata = csr_rdata;
      INST_CSRRCI:    reg_wdata = csr_rdata;

      default:        reg_wdata = 0;
    endcase

    is_pc_jump = inst_type == INST_JUMP || inst_type == INST_JUMPR || (inst_type == INST_BRANCH && com_res);
    if (pc_wen) begin
       if (is_pc_jump) pc_next = alu_res;
       else pc_next = pc_inc;
    end
    else pc_next = pc;
  end

  assign io_lsu_size  = lsu_size;
  assign io_lsu_addr  = lsu_addr;
  assign io_lsu_wdata = lsu_wdata;
  assign io_lsu_wmask = lsu_wmask;
  assign io_ifu_addr  = pc;

`ifdef verilator
reg [119:0] dbg_inst_type;

always @ *
begin
    case (inst_type)
      INST_EBREAK     : dbg_inst_type = "INST_EBREAK";
      INST_CSRRW      : dbg_inst_type = "INST_CSRRW";
      INST_CSRRS      : dbg_inst_type = "INST_CSRRS";
      INST_CSRRC      : dbg_inst_type = "INST_CSRRC";
      INST_CSRRWI     : dbg_inst_type = "INST_CSRRWI";
      INST_CSRRSI     : dbg_inst_type = "INST_CSRRSI";
      INST_CSRRCI     : dbg_inst_type = "INST_CSRRCI";

      INST_LOAD_BYTE  : dbg_inst_type = "INST_LOAD_BYTE";
      INST_LOAD_HALF  : dbg_inst_type = "INST_LOAD_HALF";
      INST_LOAD_WORD  : dbg_inst_type = "INST_LOAD_WORD";
      INST_STORE_BYTE : dbg_inst_type = "INST_STORE_BYTE";
      INST_STORE_HALF : dbg_inst_type = "INST_STORE_HALF";
      INST_STORE_WORD : dbg_inst_type = "INST_STORE_WORD";

      INST_BRANCH     : dbg_inst_type = "INST_BRANCH";
      INST_IMM        : dbg_inst_type = "INST_IMM";
      INST_REG        : dbg_inst_type = "INST_REG";
      INST_UPP        : dbg_inst_type = "INST_UPP";
      INST_JUMP       : dbg_inst_type = "INST_JUMP";
      INST_JUMPR      : dbg_inst_type = "INST_JUMPR";
      INST_AUIPC      : dbg_inst_type = "INST_AUIPC";
      default         : dbg_inst_type = "INST_UNDEFINED";
    endcase
end
`endif



endmodule


