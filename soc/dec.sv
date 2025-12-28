module dec (
  input logic [REG_END_WORD:0] inst,
  input logic clock,

  output logic [REG_END_ID:0] rd,
  output logic [REG_END_ID:0] rs1,
  output logic [REG_END_ID:0] rs2,

  output logic [REG_END_WORD:0] imm,
  output logic [3:0]            alu_op,
  output logic [3:0]            mem_wbmask,
  output logic                  is_mem_sign,
  output logic [1:0]            mem_size,
  output logic                  mem_wen,
  output logic [3:0]            inst_type
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic sign; 
  logic sub;

  logic [6:0] opcode;
  logic [2:0] funct3;

  logic [REG_END_WORD:0]   i_imm; 
  logic [REG_END_WORD:0]   u_imm; 
  logic [REG_END_WORD:0]   s_imm; 

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
    mem_wbmask = 4'b0000;
    alu_op = 0;
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
        case (funct3)
          FUNCT3_BYTE:        inst_type = {2'b10,funct3[1:0]};
          FUNCT3_HALF:        inst_type = {2'b10,funct3[1:0]};
          FUNCT3_WORD:        inst_type = {2'b10,funct3[1:0]};
          FUNCT3_BYTE_UNSIGN: inst_type = {2'b10,funct3[1:0]};
          FUNCT3_HALF_UNSIGN: inst_type = {2'b10,funct3[1:0]};
          default:            inst_type = 0;        
        endcase
      end
      OPCODE_STORE: begin
        imm = s_imm;
        case (funct3)
          FUNCT3_BYTE:        mem_wbmask = 4'b0001;
          FUNCT3_HALF:        mem_wbmask = 4'b0011;
          FUNCT3_WORD:        mem_wbmask = 4'b1111;
          default:            mem_wbmask = 4'b0000;
        endcase

        inst_type = INST_STORE;
      end
      OPCODE_LUI: begin
        imm = u_imm;
        inst_type = INST_UPP;
      end
      OPCODE_JALR: begin
        imm = i_imm;
        inst_type = INST_JUMP;
      end
      OPCODE_ENV: begin
        inst_type = 0;
        $finish();
      end
      default: inst_type = 0;
    endcase

    is_mem_sign = !funct3[2];

    case (inst_type)
      INST_LOAD_BYTE: mem_size = 2'b00;
      INST_LOAD_HALF: mem_size = 2'b01;
      INST_LOAD_WORD: mem_size = 2'b10;
      default:        mem_size = 2'b00;
    endcase

    mem_wen = inst_type == INST_STORE;
  end

endmodule;

