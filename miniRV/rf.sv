module rf (
  input logic                   clock,
  input logic                   reset,
  input logic                   wen,
  input logic  [REG_END_ID:0]   rd,
  input logic  [REG_END_ID:0]   rs1,
  input logic  [REG_END_ID:0]   rs2,
  input logic  [REG_END_WORD:0] wdata,
  output logic [N_REGS-1:0][REG_END_WORD:0] regs,
  output logic [REG_END_WORD:0] rdata1,
  output logic [REG_END_WORD:0] rdata2
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  integer i;

  always_ff @(posedge clock or posedge reset) begin
    // $display("wen: %b, rd: %d", wen, rd);
    if (reset) begin
      for (i = 0; i < N_REGS; i++) begin
        regs[i] <= 32'h0;
      end
    end else if (wen && (rd != 0) && (rd < N_REGS)) begin
      regs[rd] <= wdata;
    end
  end

  always_comb begin
    rdata1 = (rs1 == 0 || rs1 >= N_REGS) ? 32'h0 : regs[rs1];
    rdata2 = (rs2 == 0 || rs2 >= N_REGS) ? 32'h0 : regs[rs2];
  end

endmodule;


