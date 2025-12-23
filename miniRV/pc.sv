module pc #(parameter int unsigned INITIAL_PC = 32'h8000_0000) (
  input logic clk,
  input logic reset,
  input logic [31:0] in_addr,
  input logic is_addr,
  output logic [31:0] out_addr
);

  logic [31:0] next_addr;
  always_ff @(posedge clk or posedge reset) begin
    if (reset) begin
      next_addr <= INITIAL_PC;
    end else if (is_addr) begin
      next_addr <= in_addr;
    end else begin
      next_addr <= out_addr + 4;
    end
  end
  always_comb begin
    out_addr = next_addr;
  end

endmodule;


