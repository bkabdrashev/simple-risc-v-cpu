module com (
  input  logic [COM_OP_END:0] op,
  input  logic [REG_W_END:0]  lhs,
  input  logic [REG_W_END:0]  rhs,
  output logic                res);

/* verilator lint_off UNUSEDPARAM */
`include "reg_defines.vh"
`include "com_defines.vh"
/* verilator lint_on UNUSEDPARAM */

  always_comb begin
    case (op)
      COM_OP_EQ: res = lhs == rhs;
      COM_OP_NE: res = lhs != rhs;
      COM_OP_LT: res = $signed(lhs) <  $signed(rhs);
      COM_OP_GE: res = $signed(lhs) >= $signed(rhs);
      COM_OP_LTU:res = lhs <  rhs;
      COM_OP_GEU:res = lhs >= rhs;
      COM_OP_ONE:res = 1'b1;
      default:   res = 1'b0;
    endcase
  end

endmodule

