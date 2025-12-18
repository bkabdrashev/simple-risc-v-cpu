module rom (
  input logic [3:0] addr,
  output logic [7:0] inst
);

  always_comb begin
    case (addr) inside
            0: inst = 8'b10001010;
            1: inst = 8'b10010000;
            2: inst = 8'b10100000;
            3: inst = 8'b10110001;
            4: inst = 8'b00010111;
            5: inst = 8'b00101001;
            6: inst = 8'b11010001;
            7: inst = 8'b11011111;
      default: inst = 8'b01010101;
    endcase
  end

endmodule

