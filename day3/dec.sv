module dec (
  input logic [7:0] inst,
  output logic [1:0] rd,
  output logic [1:0] rs1,
  output logic [1:0] rs2,
  output logic [3:0] imm,
  output logic [1:0] opcode,
  output logic       wen,
  output logic [3:0] addr
);

  always_comb begin
    rd[0] = inst[4];
    rd[1] = inst[5];

    rs2[0] = inst[0];
    rs2[1] = inst[1];

    rs1[0] = inst[2];
    rs1[1] = inst[3];

    imm[0] = inst[0];
    imm[1] = inst[1];
    imm[2] = inst[2];
    imm[3] = inst[3];

    addr[0] = inst[2];
    addr[1] = inst[3];
    addr[2] = inst[4];
    addr[3] = inst[5];

    wen = !(inst[6] & inst[7]);

    opcode[0] = inst[6];
    opcode[1] = inst[7];
  end

endmodule;

