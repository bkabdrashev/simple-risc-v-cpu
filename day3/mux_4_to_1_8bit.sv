module Mux_8bit_4to1 (
  input logic [7:0] a,
  input logic [7:0] b,
  input logic [7:0] c,
  input logic [7:0] d,
  input logic [1:0] sel,
  output logic [7:0] out,
);


  always_comb begin
    if (sel == 2'b00) begin
      out = a;
    end else if (sel == 2'b01) begin
      out = b;
    end else if (sel == 2'b10) begin
      out = c;
    end else if (sel == 2'b11) begin
      out = d;
    end
  end

endmodule
