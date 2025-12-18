module rf (
  input logic       clk,
  input logic       reset,
  input logic       wen,
  input logic [1:0] rd,
  input logic [7:0] wdata,
  input logic [1:0] rs1,
  input logic [1:0] rs2,
  output logic [7:0] rdata1,
  output logic [7:0] rdata2,
  output logic [7:0] regs_out [0:3]
);

  logic [7:0] regs [0:3];

  always_ff @(posedge clk or posedge reset) begin
    if (reset) regs <= '{0, 0, 0, 0};
    else if (wen) regs[rd] <= wdata;
  end

  always_comb begin
    rdata1 = regs[rs1];
    rdata2 = regs[rs2];
    regs_out = regs;
  end

endmodule;

