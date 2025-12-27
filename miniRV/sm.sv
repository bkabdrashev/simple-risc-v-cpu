module sm (
  input  logic clock,
  input  logic reset,
  input  logic in,
  input  logic reqValid,
  input  logic respValid,
  output logic out
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic next;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) out <= BUS_IDLE;
    else       out <= next;
  end
  always_comb begin
    unique case (in)
      BUS_IDLE: next = reqValid  ? BUS_WAIT : BUS_IDLE;
      BUS_WAIT: next = respValid ? BUS_IDLE : BUS_WAIT;
      default:  next = BUS_IDLE;
    endcase
  end
endmodule


