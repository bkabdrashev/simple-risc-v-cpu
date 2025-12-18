module Priority_encoder (
  input logic in1,
  input logic in2,
  input logic in3,
  input logic in4,
  input logic in5,
  input logic in6,
  input logic in7,
  input logic in8,
  input logic in9,
  output logic [3:0] out
);

  always_comb begin
    if (in9) begin
      out = 4'b1001;
    end else if (in8) begin
      out = 4'b1000;
    end else if (in7) begin
      out = 4'b0111;
    end else if (in6) begin
      out = 4'b0110;
    end else if (in5) begin
      out = 4'b0101;
    end else if (in4) begin
      out = 4'b0100;
    end else if (in3) begin
      out = 4'b0011;
    end else if (in2) begin
      out = 4'b0010;
    end else if (in2) begin
      out = 4'b0001;
    end else begin
      out = 4'b0000;
    end
  end

endmodule


