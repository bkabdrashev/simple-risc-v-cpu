module alu (
  input logic [1:0] opcode,
  input logic [7:0] r0,
  input logic [7:0] rdata1,
  input logic [7:0] rdata2,
  input logic [7:0] imm,
  output logic [7:0] rout,
  output logic is_ner0
);

  always_comb begin
    if (opcode == 2'b00) begin
      rout = rdata1 + rdata2;
    end else if (opcode == 2'b10) begin
      rout = imm;
    end else begin 
      rout = 8'b10101010;
    end
    is_ner0 = r0 != rdata2;
  end

endmodule;



