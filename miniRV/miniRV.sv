module miniRV (
  input logic clock,
  input logic reset,
  input logic                  top_mem_wen,
  input logic [REG_END_WORD:0] top_mem_wdata,
  input logic [REG_END_WORD:0] top_mem_addr,

  output logic [REG_END_WORD:0] regs [0:N_REGS-1],
  output logic [REG_END_WORD:0] pc,

  output logic ebreak,
  output logic is_step_finished
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [REG_END_WORD:0] inst;
  logic [REG_END_ID:0]   rd;
  logic [REG_END_ID:0]   rs1;
  logic [REG_END_ID:0]   rs2;
  logic [REG_END_WORD:0] imm;

  logic [REG_END_WORD:0] pc_next;
  logic [REG_END_WORD:0] pc_inc;

  logic [REG_END_WORD:0] alu_res;
  logic [REG_END_WORD:0] alu_rhs;
  logic [3:0]            alu_op;

  logic [REG_END_WORD:0] reg_wdata;
  logic [REG_END_WORD:0] reg_rdata1;
  logic [REG_END_WORD:0] reg_rdata2;

  logic [REG_END_WORD:0] mem_addr;
  logic [REG_END_WORD:0] mem_wdata;
  logic [REG_END_WORD:0] mem_rdata;
  logic [3:0]  mem_wbmask;
  logic is_mem_sign;

  logic mem_byte_sign;
  logic mem_half_sign;

  logic [REG_END_WORD-REG_END_BYTE-1:0] mem_byte_extend;
  logic [REG_END_WORD-REG_END_HALF-1:0] mem_half_extend;

  logic [3:0]  inst_type;
  logic [N_REGS-1:0][REG_END_WORD:0] rf_regs_out;

  logic ifu_reqValid;
  logic lsu_reqValid;
  logic ifu_respValid;
  logic lsu_respValid;

  logic lsu_wen;
  logic reg_wen;
  logic pc_wen;

  pc u_pc(
    .clock(clock),
    .reset(reset),
    .in_addr(pc_next),
    .out_addr(pc));

  ram u_ram(
    .clock(clock),
    .reset(reset),
    .reqValid(lsu_reqValid),
    .wen(lsu_wen),
    .wdata(mem_wdata),
    .wbmask(mem_wbmask | {4{top_mem_wen}}),
    .addr(mem_addr),

    .respValid(lsu_respValid),
    .rdata(mem_rdata));

  mread u_mread(
    .clock(clock),
    .reset(reset),
    .reqValid(ifu_reqValid),
    .addr(pc),

    .respValid(ifu_respValid),
    .rdata(inst));

  dec u_dec(
    .inst(inst),
    .clock(clock),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .is_mem_sign(is_mem_sign),
    .alu_op(alu_op),
    .imm(imm),
    .ebreak(ebreak),
    .mem_wbmask(mem_wbmask),
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
    .rdata2(reg_rdata2),
    .regs(rf_regs_out));

  sm u_sm(
    .clock(clock),
    .reset(reset),
    .top_mem_wen(top_mem_wen),

    .ifu_respValid(ifu_respValid),
    .lsu_respValid(lsu_respValid),
    .inst_type(inst_type),

    .pc_wen(pc_wen),
    .reg_wen(reg_wen),
    .lsu_wen(lsu_wen),

    .ifu_reqValid(ifu_reqValid),
    .lsu_reqValid(lsu_reqValid),
    .finished(is_step_finished));

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

    if (top_mem_wen) begin
      mem_addr  = top_mem_addr;
      mem_wdata = top_mem_wdata;
    end
    else begin
      mem_addr = alu_res;
      mem_wdata = reg_rdata2;
    end

    mem_byte_sign = mem_rdata[REG_END_BYTE] & is_mem_sign;
    mem_half_sign = mem_rdata[REG_END_HALF] & is_mem_sign;
    mem_byte_extend = {(REG_END_WORD-REG_END_BYTE){mem_byte_sign}};
    mem_half_extend = {(REG_END_WORD-REG_END_HALF){mem_half_sign}};
    case (inst_type)
      INST_LOAD_BYTE: reg_wdata = {mem_byte_extend, mem_rdata[REG_END_BYTE:0]};
      INST_LOAD_HALF: reg_wdata = {mem_half_extend, mem_rdata[REG_END_HALF:0]};
      INST_LOAD_WORD: reg_wdata = mem_rdata;
      INST_UPP:       reg_wdata = imm;
      INST_JUMP:      reg_wdata = pc_inc;         
      INST_REG:       reg_wdata = alu_res;        
      INST_IMM:       reg_wdata = alu_res;        
      default:        reg_wdata = 0;
    endcase

    if (pc_wen) begin
       if (inst_type == INST_JUMP) pc_next = alu_res & ~3;
       else pc_next = pc_inc;
    end
    else pc_next = pc;

    for (int i = 0; i < N_REGS; i++) begin
      regs[i] = rf_regs_out[i];
    end
  end

endmodule;


