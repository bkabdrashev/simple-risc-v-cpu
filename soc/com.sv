module com (
  input  logic [2:0]            op,
  input  logic [REG_END_WORD:0] lhs,
  input  logic [REG_END_WORD:0] rhs,
  output logic                  res
);
/* verilator lint_off UNUSEDPARAM */
  `include "./soc/defs.vh"
/* verilator lint_on UNUSEDPARAM */

  always_comb begin
    case (op)
      COM_OP_EQ: res = lhs == rhs;
      COM_OP_NE: res = lhs != rhs;
      COM_OP_LT: res = $signed(lhs) <  $signed(rhs);
      COM_OP_GE: res = $signed(lhs) >= $signed(rhs);
      COM_OP_LTU:res = lhs <  rhs;
      COM_OP_GEU:res = lhs >= rhs;
      default:   res = 1'b0; // not implemented
    endcase
  end

endmodule;

