module dec (
  input logic [31:0] inst,

  output logic [4:0] rd,
  output logic [4:0] rs1,
  output logic [4:0] rs2,
  output logic [6:0] opcode,
  output logic [2:0] funct3,
  output logic       wen,
  output logic [31:0] imm
);

  logic sign; 

  always_comb begin
    opcode = inst[6:0];
    rd = inst[11:7];
    funct3 = inst[14:12];
    rs1 = inst[19:15];
    rs2 = inst[24:20];

    sign = inst[31];
    if (opcode == 7'b0010011) begin
      // ADDI
      imm = {{20{sign}}, inst[31:20]};
      wen = 1;
    end else if (opcode == 7'b1100111) begin
      // JALR
      imm = {{20{sign}}, inst[31:20]};
      wen = 1;
    end else if (opcode == 7'b0110011) begin
      // ADD
      imm = {{12{sign}}, inst[31:12]};
      wen = 1;
    end else if (opcode == 7'b0110111) begin
      // LUI
      imm = { inst[31:12], 12'd0 };
      wen = 1;
    end else if (opcode == 7'b0000011) begin
      // LW, LBU
      imm = {{20{sign}}, inst[31:20]};
      wen = 1;
    end else if (opcode == 7'b0100011) begin
      // SW, SB
      imm = {{20{sign}}, inst[31:25], inst[11:7]};
      wen = 0;
    end else if (opcode == 7'b1110011) begin
      // ECALL, EBREAK
      imm = {20'b0, inst[31:20]};
      wen = 0;
    end else begin
      imm = 0;
      wen = 0;
    end
  end

endmodule;

