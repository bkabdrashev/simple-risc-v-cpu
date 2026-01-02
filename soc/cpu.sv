module cpu (
  input         clock,
  input         reset,

  input  [31:0] io_ifu_rdata,
  input         io_ifu_respValid,
  output        io_ifu_reqValid,
  output [31:0] io_ifu_addr,

  input         io_lsu_respValid,
  input  [31:0] io_lsu_rdata,
  output        io_lsu_reqValid,
  output [31:0] io_lsu_addr,
  output [1:0]  io_lsu_size,
  output        io_lsu_wen,
  output [31:0] io_lsu_wdata,
  output [3:0]  io_lsu_wmask,
  output logic [REG_END_WORD:0] regs [0:N_REGS-1],
  output logic [REG_END_WORD:0] pc,
  output logic                  ebreak,
  output logic                  is_done_instruction);
/* verilator lint_off UNUSEDPARAM */
  `include "./soc/defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [REG_END_ID:0]   rd;
  logic [REG_END_ID:0]   rs1;
  logic [REG_END_ID:0]   rs2;
  logic [REG_END_WORD:0] imm;

  logic                  pc_wen;
  logic [REG_END_WORD:0] pc_next;
  logic [REG_END_WORD:0] pc_inc;

  logic [REG_END_WORD:0] alu_res;
  logic [REG_END_WORD:0] alu_rhs;
  logic [3:0]            alu_op;

  logic [REG_END_WORD:0] reg_wdata;
  logic [REG_END_WORD:0] reg_rdata1;
  logic [REG_END_WORD:0] reg_rdata2;

  logic is_mem_sign;

  logic mem_byte_sign;
  logic mem_half_sign;

  logic [REG_END_WORD-REG_END_BYTE-1:0] mem_byte_extend;
  logic [REG_END_WORD-REG_END_HALF-1:0] mem_half_extend;

  logic [3:0]  inst_type;

  logic [N_REGS-1:0][REG_END_WORD:0] rf_regs;
  logic reg_wen;
  logic [7:0]  byte2;
  logic [15:0] half2;
  logic [3:0] lsu_wmask;
  logic [31:0] lsu_wdata;
  logic [31:0] lsu_rdata;
  logic [31:0] lsu_addr;
  logic [1:0]  lsu_size;

  pc u_pc(
    .clock(clock),
    .reset(reset),
    .in_addr(pc_next),
    .out_addr(pc));

  dec u_dec(
    .inst(io_ifu_rdata),
    .clock(clock),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .is_mem_sign(is_mem_sign),
    .alu_op(alu_op),
    .imm(imm),
    .inst_type(inst_type));

  alu u_alu(
    .op(alu_op),
    .lhs(reg_rdata1),
    .rhs(alu_rhs),
    .res(alu_res));

  rf u_rf(
    .clock(clock),
    .reset(reset),

    .wen(reg_wen),
    .wdata(reg_wdata),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .rdata1(reg_rdata1),
    .rdata2(reg_rdata2),
    .regs(rf_regs));

  sm u_sm(
    .clock(clock),
    .reset(reset),

    .ifu_respValid(io_ifu_respValid),
    .lsu_respValid(io_lsu_respValid),
    .inst_type(inst_type),

    .reg_wen(reg_wen),
    .pc_wen(pc_wen),
    .lsu_wen(io_lsu_wen),

    .ifu_reqValid(io_ifu_reqValid),
    .lsu_reqValid(io_lsu_reqValid),
    .finished(is_done_instruction),
    .ebreak(ebreak));

  always_comb begin
    pc_inc = pc + 4;

    case (inst_type)
      INST_REG:        alu_rhs = reg_rdata2;        
      INST_LOAD_BYTE:  alu_rhs = imm;
      INST_LOAD_HALF:  alu_rhs = imm;
      INST_LOAD_WORD:  alu_rhs = imm;
      INST_STORE_BYTE: alu_rhs = imm;
      INST_STORE_HALF: alu_rhs = imm;
      INST_STORE_WORD: alu_rhs = imm;
      INST_JUMP:       alu_rhs = imm;         
      INST_IMM:        alu_rhs = imm;        
      INST_UPP:        alu_rhs = 0;        
      default:         alu_rhs = 0;
    endcase

    byte2 = reg_rdata2[7:0];
    half2 = reg_rdata2[15:0];

    lsu_size  = 0;
    lsu_wdata = 0;
    lsu_rdata = 0;
    lsu_wmask = 0;
    lsu_addr = alu_res;

    case (inst_type)
      INST_STORE_BYTE: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b0001; lsu_wdata = {8'b0, 8'b0, 8'b0, byte2}; end
          2'b01: begin lsu_wmask = 4'b0010; lsu_wdata = {8'b0, 8'b0, byte2, 8'b0}; end
          2'b10: begin lsu_wmask = 4'b0100; lsu_wdata = {8'b0, byte2, 8'b0, 8'b0}; end
          2'b11: begin lsu_wmask = 4'b1000; lsu_wdata = {byte2, 8'b0, 8'b0, 8'b0}; end
        endcase
      end
      INST_STORE_HALF: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b0011; lsu_wdata = {8'b0, 8'b0, half2}; end
          2'b01: begin lsu_wmask = 4'b0110; lsu_wdata = {8'b0, half2, 8'b0}; end
          2'b10: begin lsu_wmask = 4'b1100; lsu_wdata = {half2, 8'b0, 8'b0}; end
          2'b11: begin lsu_wmask = 4'b0000; lsu_wdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      INST_STORE_WORD: begin
        case (alu_res[1:0])
          2'b00: begin lsu_wmask = 4'b1111; lsu_wdata = reg_rdata2; end
          2'b01: begin lsu_wmask = 4'b0000; lsu_wdata = 32'b0; end // TODO: exceptional case
          2'b10: begin lsu_wmask = 4'b0000; lsu_wdata = 32'b0; end // TODO: exceptional case
          2'b11: begin lsu_wmask = 4'b0000; lsu_wdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      INST_LOAD_BYTE: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = {24'b0, io_lsu_rdata[ 7: 0]}; end
          2'b01: begin lsu_rdata = {24'b0, io_lsu_rdata[15: 8]}; end
          2'b10: begin lsu_rdata = {24'b0, io_lsu_rdata[23:16]}; end
          2'b11: begin lsu_rdata = {24'b0, io_lsu_rdata[31:24]}; end
        endcase
      end
      INST_LOAD_HALF: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = {16'b0, io_lsu_rdata[15: 0]}; end
          2'b01: begin lsu_rdata = {16'b0, io_lsu_rdata[23: 8]}; end
          2'b10: begin lsu_rdata = {16'b0, io_lsu_rdata[31:16]}; end
          2'b11: begin lsu_rdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      INST_LOAD_WORD: begin
        lsu_size = 2'b10;
        case (alu_res[1:0])
          2'b00: begin lsu_rdata = io_lsu_rdata; end
          2'b01: begin lsu_rdata = 32'b0; end // TODO: exceptional case
          2'b10: begin lsu_rdata = 32'b0; end // TODO: exceptional case
          2'b11: begin lsu_rdata = 32'b0; end // TODO: exceptional case
        endcase
      end
      default:  begin end
    endcase
    
    mem_byte_sign   = lsu_rdata[REG_END_BYTE] & is_mem_sign;
    mem_half_sign   = lsu_rdata[REG_END_HALF] & is_mem_sign;
    mem_byte_extend = {(REG_END_WORD-REG_END_BYTE){mem_byte_sign}};
    mem_half_extend = {(REG_END_WORD-REG_END_HALF){mem_half_sign}};
    case (inst_type)
      INST_LOAD_BYTE: reg_wdata = {mem_byte_extend, lsu_rdata[REG_END_BYTE:0]};
      INST_LOAD_HALF: reg_wdata = {mem_half_extend, lsu_rdata[REG_END_HALF:0]};
      INST_LOAD_WORD: reg_wdata = lsu_rdata;
      INST_UPP:       reg_wdata = imm;
      INST_JUMP:      reg_wdata = pc_inc;         
      INST_REG:       reg_wdata = alu_res;        
      INST_IMM:       reg_wdata = alu_res;        
      default:        reg_wdata = 0;
    endcase

    if (pc_wen) begin
       if (inst_type == INST_JUMP) pc_next = alu_res;
       else pc_next = pc_inc;
    end
    else pc_next = pc;
    for (int i = 0; i < N_REGS; i++) begin
      regs[i] = rf_regs[i];
    end

  end

  assign io_lsu_size  = lsu_size;
  assign io_lsu_addr  = lsu_addr;
  assign io_lsu_wdata = lsu_wdata;
  assign io_lsu_wmask = lsu_wmask;
  assign io_ifu_addr  = pc;

endmodule;


