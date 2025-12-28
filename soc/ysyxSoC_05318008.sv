module ysyxSoC_05318008 (
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

  logic [REG_END_ID:0]   rd;
  logic [REG_END_ID:0]   rs1;
  logic [REG_END_ID:0]   rs2;
  logic [REG_END_WORD:0] imm;

  logic [REG_END_WORD:0] pc;
  logic                  pc_wen;
  logic [REG_END_WORD:0] pc_next;
  logic [REG_END_WORD:0] pc_inc;

  logic [REG_END_WORD:0] alu_res;
  logic [REG_END_WORD:0] alu_rhs;
  logic [3:0]            alu_op;

  logic [REG_END_WORD:0] reg_wdata;
  logic [REG_END_WORD:0] reg_rdata1;
  logic [REG_END_WORD:0] reg_rdata2;

  logic is_mem_sign;

  logic mem_byte_sign;
  logic mem_half_sign;

  logic [REG_END_WORD-REG_END_BYTE-1:0] mem_byte_extend;
  logic [REG_END_WORD-REG_END_HALF-1:0] mem_half_extend;

  logic [3:0]  inst_type;

  logic [2:0] state;
  logic       is_step_finished;

  logic reg_wen;

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
    .imm(imm),
    .mem_wbmask(io_lsu_wmask),
    .mem_size(io_lsu_size),
    .mem_wen(io_lsu_wen),
    .inst_type(inst_type));

  alu u_alu(
    .op(alu_op),
    .lhs(reg_rdata1),
    .rhs(alu_rhs),
    .res(alu_res));

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

  sm u_sm(
    .clock(clock),
    .reset(reset),

    .in(state),
    .ifu_respValid(io_ifu_respValid),
    .lsu_respValid(io_lsu_respValid),
    .inst_type(inst_type),

    .reg_wen(reg_wen),
    .pc_wen(pc_wen),

    .ifu_reqValid(io_ifu_reqValid),
    .lsu_reqValid(io_lsu_reqValid),
    .finished(is_step_finished),
    .out(state));

  always_comb begin
    pc_inc = pc + 4;

    case (inst_type)
      INST_REG:       alu_rhs = reg_rdata2;        
      INST_LOAD_BYTE: alu_rhs = imm;
      INST_LOAD_HALF: alu_rhs = imm;
      INST_LOAD_WORD: alu_rhs = imm;
      INST_STORE:     alu_rhs = imm;
      INST_JUMP:      alu_rhs = imm;         
      INST_IMM:       alu_rhs = imm;        
      INST_UPP:       alu_rhs = 0;        
      default:        alu_rhs = 0;
    endcase

    mem_byte_sign   = io_lsu_rdata[REG_END_BYTE] & is_mem_sign;
    mem_half_sign   = io_lsu_rdata[REG_END_HALF] & is_mem_sign;
    mem_byte_extend = {(REG_END_WORD-REG_END_BYTE){mem_byte_sign}};
    mem_half_extend = {(REG_END_WORD-REG_END_HALF){mem_half_sign}};
    case (inst_type)
      INST_LOAD_BYTE: reg_wdata = {mem_byte_extend, io_lsu_rdata[REG_END_BYTE:0]};
      INST_LOAD_HALF: reg_wdata = {mem_half_extend, io_lsu_rdata[REG_END_HALF:0]};
      INST_LOAD_WORD: reg_wdata = io_lsu_rdata;
      INST_UPP:       reg_wdata = imm;
      INST_JUMP:      reg_wdata = pc;         
      INST_REG:       reg_wdata = alu_res;        
      INST_IMM:       reg_wdata = alu_res;        
      default:        reg_wdata = 0;
    endcase

    if (pc_wen) begin
       if (inst_type == INST_JUMP) pc_next = alu_res & ~3;
       else pc_next = pc_inc;
    end
    else pc_next = pc;

  end
  assign io_lsu_addr  = alu_res;
  assign io_lsu_wdata = reg_rdata2;
  assign io_ifu_addr = pc;

endmodule;


