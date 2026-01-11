module exu (
  input  logic               clock,
  input  logic               reset,
  input  logic               reqValid,
  output logic               respValid,

  input  logic               lsu_respValid,
  input  logic               is_lsu_inst,
  output logic               is_inst_retired,

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

  typedef enum logic [2:0] { EXU_START, EXU_RESET, EXU_EXECUTE, EXU_STALL_IDU, EXU_STALL_LSU } cpu_state;

  cpu_state next_state;
  cpu_state curr_state;
  cpu_state lsu_or_exec;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      curr_state <= EXU_RESET;
      is_inst_retired <= 1'b0;
    end else begin
      curr_state <= next_state;
      is_inst_retired <= respValid;
    end
  end

  assign lsu_or_exec = is_lsu_inst && ~lsu_respValid ? EXU_STALL_LSU : EXU_EXECUTE;
  assign respValid   = next_state == EXU_EXECUTE;

  always_comb begin
    next_state = curr_state;
    unique case (curr_state)
      EXU_RESET: begin
        next_state = EXU_START;
      end
      EXU_START: begin
        next_state = EXU_STALL_IDU;
        if (reqValid) begin
          next_state = lsu_or_exec;
        end
      end
      EXU_STALL_IDU: begin
        if (reqValid) begin
          next_state = lsu_or_exec;
        end
        else begin
          next_state = EXU_STALL_IDU;
        end
      end
      EXU_STALL_LSU: begin
        if (lsu_respValid) begin
          next_state = EXU_EXECUTE;
        end
        else begin
          next_state = EXU_STALL_LSU;
        end
      end
      EXU_EXECUTE: begin
        next_state = EXU_STALL_IDU;
        if (reqValid) begin
          next_state = lsu_or_exec;
        end
      end
    endcase
  end

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

`ifdef verilator
/* verilator lint_off UNUSEDSIGNAL */
reg [103:0]  dbg_exu;
always @ * begin
  case (curr_state)
    EXU_RESET:     dbg_exu = "EXU_RESET";
    EXU_START:     dbg_exu = "EXU_START";
    EXU_STALL_IDU: dbg_exu = "EXU_STALL_IDU";
    EXU_STALL_LSU: dbg_exu = "EXU_STALL_LSU";
    EXU_EXECUTE:   dbg_exu = "EXU_EXECUTE";
    default:       dbg_exu = "EXU_NONE";
  endcase
end
/* verilator lint_on UNUSEDSIGNAL */
`endif
endmodule
