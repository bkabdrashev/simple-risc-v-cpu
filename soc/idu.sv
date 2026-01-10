module idu (
  input  logic [REG_W_END:0]     inst,

  output logic [REG_A_END:0]     rd,
  output logic [REG_A_END:0]     rs1,
  output logic [REG_A_END:0]     rs2,

  output logic [REG_W_END:0]     imm,
  output logic [ALU_OP_END:0]    alu_op,
  output logic [COM_OP_END:0]    com_op,
  output logic [INST_TYPE_END:0] inst_type);

/* verilator lint_off UNUSEDPARAM */
  `include "com_defines.vh"
  `include "reg_defines.vh"
  `include "alu_defines.vh"
  `include "inst_defines.vh"
/* verilator lint_on UNUSEDPARAM */

  localparam FUNCT3_SR        = 3'b101;
  localparam FUNCT3_ADD       = 3'b000;

  localparam OPCODE_LUI       = 7'b0110111;
  localparam OPCODE_AUIPC     = 7'b0010111;
  localparam OPCODE_JAL       = 7'b1101111;
  localparam OPCODE_JALR      = 7'b1100111;
  localparam OPCODE_BRANCH    = 7'b1100011;
  localparam OPCODE_LOAD      = 7'b0000011;
  localparam OPCODE_STORE     = 7'b0100011;
  localparam OPCODE_CALC_IMM  = 7'b0010011;
  localparam OPCODE_CALC_REG  = 7'b0110011;
  localparam OPCODE_SYSTEM    = 7'b1110011;

  logic [6:0]         opcode;
  logic [2:0]         funct3;
  logic               sign;
  logic               sub;
  logic [REG_W_END:0] i_imm;
  logic [REG_W_END:0] u_imm;
  logic [REG_W_END:0] s_imm;
  logic [REG_W_END:0] j_imm;
  logic [REG_W_END:0] b_imm;

  assign opcode = inst[6:0];
  assign rd     = inst[11:7];
  assign funct3 = inst[14:12];
  assign rs1    = inst[19:15];
  assign rs2    = inst[24:20];
  assign sign   = inst[31];
  assign sub    = inst[30];
  assign i_imm  = { {20{sign}}, inst[31:20] };
  assign u_imm  = { inst[31:12], 12'd0 };
  assign s_imm  = { {20{sign}}, inst[31:25], inst[11:7] };
  assign j_imm  = { {12{sign}}, inst[19:12], inst[20], inst[30:21], 1'b0 };
  assign b_imm  = { {20{sign}}, inst[7], inst[30:25], inst[11:8], 1'b0  };

  always_comb begin
    imm = 0;
    alu_op = ALU_OP_ADD;
    com_op = COM_OP_ONE;
    case (opcode)
      OPCODE_CALC_IMM: begin
        imm = i_imm;
        inst_type = INST_IMM;
        alu_op = {sub & funct3==FUNCT3_SR,funct3};
      end
      OPCODE_CALC_REG: begin
        inst_type = INST_REG;
        alu_op = {sub & (funct3==FUNCT3_ADD || funct3 == FUNCT3_SR),funct3};
      end
      OPCODE_LOAD: begin
        imm = i_imm;
        inst_type = {INST_LOAD,funct3};
      end
      OPCODE_STORE: begin
        imm = s_imm;
        inst_type = {INST_STORE,funct3};
      end
      OPCODE_LUI: begin
        imm = u_imm;
        alu_op = ALU_OP_RHS;
        inst_type = INST_UPP;
      end
      OPCODE_AUIPC: begin
        imm = u_imm;
        inst_type = INST_AUIPC;
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
      // WRITE a = b
      // SET   a = b | c
      // CLEAR a = b & ~c
        imm       = i_imm;
        alu_op    = {funct3[0],funct3[1],2'b10};
        inst_type = {INST_SYSTEM, funct3[2], 1'b0, |funct3[1:0]};
      end
      default: inst_type = 0;
    endcase
  end

endmodule

