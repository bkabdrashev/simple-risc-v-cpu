module dec (
  input logic [REG_END_WORD:0]   inst,
  input logic                    clock,

  output logic [REG_END_ID:0]    rd,
  output logic [REG_END_ID:0]    rs1,
  output logic [REG_END_ID:0]    rs2,

  output logic [REG_END_WORD:0]  imm,
  output logic [ALU_OP_END:0]    alu_op,
  output logic [COM_OP_END:0]    com_op,
  output logic [INST_TYPE_END:0] inst_type);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

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

