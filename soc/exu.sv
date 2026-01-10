module exu (
  input  logic               reqValid,
  output logic               respValid,
  input  logic [REG_W_END:0] lsu_rdata,
  input  logic [REG_W_END:0] csr_rdata,
  input  logic [REG_W_END:0] rdata1,
  input  logic [REG_W_END:0] rdata2,
  input  logic [REG_W_END:0] pc,
  input  logic [REG_W_END:0] pc_inc,

  output logic               is_pc_jump,
  output logic               rf_wen,
  output logic               csr_wen,
  output logic [REG_W_END:0] pc_jump,
  output logic [REG_W_END:0] rf_wdata,
  output logic [REG_W_END:0] csr_wdata,
  output logic [REG_W_END:0] lsu_addr,
  output logic [REG_W_END:0] lsu_wdata,

  input  logic [REG_A_END:0]     csr_imm,
  input  logic [ALU_OP_END:0]    alu_op,
  input  logic [COM_OP_END:0]    com_op,
  input  logic [REG_W_END:0]     imm,
  input  logic [INST_TYPE_END:0] inst_type);

/* verilator lint_off UNUSEDPARAM */
`include "com_defines.vh"
`include "reg_defines.vh"
`include "alu_defines.vh"
`include "inst_defines.vh"
/* verilator lint_on UNUSEDPARAM */

// NOTE: this is true, since all instructions done in EXU are 1 cycle, so 0 cycle stall
  assign respValid = reqValid;

  logic [REG_W_END:0] alu_lhs;
  logic [REG_W_END:0] alu_rhs;
  logic [REG_W_END:0] alu_res;
  logic               com_res;

  alu u_alu(
    .op(alu_op),
    .lhs(alu_lhs),
    .rhs(alu_rhs),
    .res(alu_res));

  com u_com(
    .op(com_op),
    .lhs(rdata1),
    .rhs(rdata2),
    .res(com_res));

  always_comb begin
    case (inst_type)
      INST_CSRI:   alu_lhs = {27'b0,  csr_imm};
      INST_JUMP:   alu_lhs = pc;
      INST_AUIPC:  alu_lhs = pc;
      INST_BRANCH: alu_lhs = pc;
      default:     alu_lhs = rdata1;
    endcase
  end

  always_comb begin
    case (inst_type)
      INST_REG:  alu_rhs = rdata2;
      INST_CSR:  alu_rhs = csr_rdata;
      INST_CSRI: alu_rhs = csr_rdata;
      default:   alu_rhs = imm;
    endcase
  end

  always_comb begin
    case (inst_type)
      INST_JUMP:    rf_wdata = pc_inc;
      INST_JUMPR:   rf_wdata = pc_inc;

      INST_UPP:     rf_wdata = alu_res;
      INST_AUIPC:   rf_wdata = alu_res;
      INST_REG:     rf_wdata = alu_res;
      INST_IMM:     rf_wdata = alu_res;

      INST_CSR:     rf_wdata = csr_rdata;
      INST_CSRI:    rf_wdata = csr_rdata;

      INST_LOAD_B:  rf_wdata = lsu_rdata;
      INST_LOAD_H:  rf_wdata = lsu_rdata;
      INST_LOAD_W:  rf_wdata = lsu_rdata;
      INST_LOAD_BU: rf_wdata = lsu_rdata;
      INST_LOAD_HU: rf_wdata = lsu_rdata;
      
      default:      rf_wdata = 0;
    endcase
  end

  assign csr_wdata = alu_res;

  assign lsu_wdata = rdata2;
  assign lsu_addr  = alu_res;

  assign is_pc_jump = inst_type == INST_JUMP || inst_type == INST_JUMPR || (inst_type == INST_BRANCH && com_res);
  assign pc_jump    = alu_res;

  assign rf_wen     = inst_type[3] && respValid;
  assign csr_wen    = inst_type[5] && respValid;

endmodule
