module alu (
  input  logic [ALU_OP_END:0]   op,
  input  logic [REG_END_WORD:0] lhs,
  input  logic [REG_END_WORD:0] rhs,
  output logic [REG_END_WORD:0] res
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [4:0] shamt;

  always_comb begin
    shamt = rhs[4:0];
    case (op)
      ALU_OP_ADD: res = lhs + rhs;
      ALU_OP_SUB: res = lhs - rhs;
      ALU_OP_XOR: res = lhs ^ rhs;
      ALU_OP_AND: res = lhs & rhs;
      ALU_OP_ANDN:res = lhs & ~rhs;
      ALU_OP_OR:  res = lhs | rhs;
      ALU_OP_SLL: res = lhs << shamt;
      ALU_OP_SRL: res = lhs >> shamt;
      ALU_OP_SRA: res = $signed(lhs) >>> shamt;
      ALU_OP_SLT: res = { 31'b0, $signed(lhs) < $signed(rhs) };
      ALU_OP_SLTU:res = { 31'b0, lhs < rhs };
      ALU_OP_LHS: res = lhs;
      ALU_OP_RHS: res = rhs;
      default:    res = 'b1010; // not implemented
    endcase
  end

endmodule

