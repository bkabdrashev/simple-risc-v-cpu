module pc (
  input  logic               clock,
  input  logic               reset,
  input  logic               wen,
  input  logic [REG_W_END:0] wdata,
  output logic [REG_W_END:0] rdata);
  localparam INITIAL_PC = 32'h3000_0000;
/* verilator lint_off UNUSEDPARAM */
  `include "reg_defines.vh"
/* verilator lint_on UNUSEDPARAM */
  always_ff @(posedge clock or posedge reset) begin
    /**/ if (reset) rdata <= INITIAL_PC;
    else if (wen)   rdata <= wdata;
  end

endmodule


