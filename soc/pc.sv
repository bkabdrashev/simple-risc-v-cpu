module pc (
  input  logic                  clock,
  input  logic                  reset,
  input  logic                  wen,
  input  logic [REG_END_WORD:0] wdata,
  output logic [REG_END_WORD:0] rdata);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  always_ff @(posedge clock or posedge reset) begin
    /**/ if (reset) rdata <= INITIAL_PC;
    else if (wen)   rdata <= wdata;
  end

endmodule


