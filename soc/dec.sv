module dec (
  input logic [REG_END_WORD:0] inst,
  input logic clock,

  output logic [REG_END_ID:0] rd,
  output logic [REG_END_ID:0] rs1,
  output logic [REG_END_ID:0] rs2,

  output logic [REG_END_WORD:0]  imm,
  output logic [3:0]             alu_op,
  output logic [2:0]             com_op,
  output logic                   is_mem_sign,
  output logic [INST_TYPE_END:0] inst_type);
/* verilator lint_off UNUSEDPARAM */
  `include "./soc/defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic sign; 
  logic sub;

  logic [6:0] opcode;
  logic [2:0] funct3;

  logic [REG_END_WORD:0] i_imm;
  logic [REG_END_WORD:0] u_imm;
  logic [REG_END_WORD:0] s_imm;
  logic [REG_END_WORD:0] j_imm;
  logic [REG_END_WORD:0] b_imm;

  always_comb begin
    opcode = inst[6:0];
    rd     = inst[11:7];
    funct3 = inst[14:12];
    rs1    = inst[19:15];
    rs2    = inst[24:20];

    sign = inst[31];
    sub = inst[30];

    imm = 0;

    i_imm = { {20{sign}}, inst[31:20] };
    u_imm = { inst[31:12], 12'd0 };
    s_imm = { {20{sign}}, inst[31:25], inst[11:7] };
    j_imm = { {12{sign}}, inst[19:12], inst[20], inst[30:21], 1'b0};
    b_imm = { {20{sign}}, inst[7], inst[30:25], inst[11:8], 1'b0 };

    alu_op = ALU_OP_ADD;
    com_op = COM_OP_EQ;
    case (opcode)
      OPCODE_CALC_IMM: begin
        imm = i_imm;
        inst_type = INST_IMM;
        alu_op = {sub & funct3==FUNCT3_SR,funct3};
      end
      OPCODE_CALC_REG: begin
        inst_type = INST_REG;
        alu_op = {sub,funct3};
      end
      OPCODE_LOAD: begin
        imm = i_imm;
        inst_type = {3'b011,funct3[1:0]};
      end
      OPCODE_STORE: begin
        imm = s_imm;
        inst_type = {3'b010,funct3[1:0]};
      end
      OPCODE_LUI: begin
        imm = u_imm;
        inst_type = INST_UPP;
      end
      OPCODE_JAL: begin
        imm = j_imm;
        inst_type = INST_JUMP;
      end
      OPCODE_JALR: begin
        imm = i_imm;
        inst_type = INST_JUMPR;
      end
      OPCODE_BRANCH: begin
        imm = b_imm;
        com_op    = funct3;
        inst_type = INST_BRANCH;
      end
      OPCODE_SYSTEM: begin
      // WRITE a = b >> 0 
      // SET   a = b | c
      // CLEAR a = b & ~c
        imm       = i_imm;
        alu_op    = {2'b01, funct3[1:0]};
        inst_type = {2'b10, funct3};
      end
      default: inst_type = 0;
    endcase

    is_mem_sign = !funct3[2];

  end

endmodule;

