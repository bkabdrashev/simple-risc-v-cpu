module rf (
  input logic                clock,
  input logic                reset,
  input logic                wen,
  input logic  [REG_A_END:0] rd,
  input logic  [REG_A_END:0] rs1,
  input logic  [REG_A_END:0] rs2,
  input logic  [REG_W_END:0] wdata,
  output logic [REG_W_END:0] rdata1,
  output logic [REG_W_END:0] rdata2);

/* verilator lint_off UNUSEDPARAM */
  `include "reg_defines.vh"
/* verilator lint_on UNUSEDPARAM */
  localparam REG_NUM = 16;

  logic [REG_W_END:0] regs [0:REG_NUM-1];
  integer i;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      for (i = 0; i < REG_NUM; i++) begin
        regs[i] <= 32'h0;
      end
    end else if (wen && (rd != 0) && (rd < REG_NUM)) begin
      regs[rd[3:0]] <= wdata;
    end
  end

  always_comb begin
    rdata1 = (rs1 == 0 || rs1 >= REG_NUM) ? 32'h0 : regs[rs1[3:0]];
    rdata2 = (rs2 == 0 || rs2 >= REG_NUM) ? 32'h0 : regs[rs2[3:0]];
  end

endmodule


