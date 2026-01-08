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
  output [3:0]  io_lsu_wmask);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic                  ebreak;
  logic                  is_done_instruction;
  logic                  is_lsu_busy;

  logic [REG_END_ID:0]   rd;
  logic [REG_END_ID:0]   rs1;
  logic [REG_END_ID:0]   rs2;
  logic [REG_END_WORD:0] imm;

  logic [REG_END_WORD:0] pc;
  logic                  pc_wen;

  logic [REG_END_WORD:0] alu_res;
  logic [REG_END_WORD:0] alu_lhs;
  logic [REG_END_WORD:0] alu_rhs;
  logic [ALU_OP_END:0]   alu_op;

  logic [COM_OP_END:0]   com_op;
  logic                  com_res;

  logic                  reg_wen;
  logic [REG_END_WORD:0] reg_wdata;
  logic [REG_END_WORD:0] reg_rdata1;
  logic [REG_END_WORD:0] reg_rdata2;

  logic [INST_TYPE_END:0] inst_type;

  logic [31:0]           lsu_rdata;

  logic [REG_END_WORD:0] csr_rdata;
  logic [REG_END_WORD:0] csr_wdata;
  logic                  csr_wen;

  assign io_ifu_addr  = pc;

  pc u_pc(
    .clock(clock),
    .reset(reset),
    .wen(pc_wen),
    .wdata(pc_next),
    .rdata(pc));

  ifu u_ifu(
    .clock(clock),
    .reset(reset),
    .inst_type(inst_type),
    .ifu_respValid(io_ifu_respValid),
    .ifu_reqValid(io_ifu_reqValid));

  dec u_dec(
    .inst(io_ifu_rdata),
    .clock(clock),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .alu_op(alu_op),
    .com_op(com_op),
    .imm(imm),
    .inst_type(inst_type));

  always_comb begin
    case (inst_type)
      INST_CSRRI:  alu_lhs = {27'b0,  rs1};
      INST_JUMP:   alu_lhs = pc;
      INST_AUIPC:  alu_lhs = pc;
      INST_BRANCH: alu_lhs = pc;
      default:     alu_lhs = reg_rdata1;
    endcase
  end

  always_comb begin
    case (inst_type)
      INST_REG:   alu_rhs = reg_rdata2;
      INST_CSRR:  alu_rhs = csr_rdata;
      INST_CSRRI: alu_rhs = csr_rdata;
      default:    alu_rhs = imm;
    endcase
  end

  alu u_alu(
    .op(alu_op),
    .lhs(alu_lhs),
    .rhs(alu_rhs),
    .res(alu_res));

  com u_com(
    .op(com_op),
    .lhs(reg_rdata1),
    .rhs(reg_rdata2),
    .res(com_res));

  rf u_rf(
    .clock(clock),
    .reset(reset),

    .wen(reg_wen),
    .wdata(reg_wdata),

    .rd(rd),
    .rs1(rs1),
    .rs2(rs2),

    .rdata1(reg_rdata1),
    .rdata2(reg_rdata2));

  assign csr_wen   = inst_type[5];
  assign csr_wdata = alu_res;
  csr u_csr(
    .clock(clock),
    .reset(reset),
    .wen(csr_wen),
    .addr(imm[11:0]),
    .is_done_inst(is_done_instruction),
    .wdata(csr_wdata),
    .rdata(csr_rdata));

  assign io_lsu_wen  = inst_type[5:4] == INST_STORE;
  assign io_lsu_size = inst_type[1:0];
  lsu u_lsu(
    .clock(clock),
    .reset(reset),

    .is_lsu     (inst_type[4]),
    .is_busy    (is_lsu_busy),
    .rdata      (io_lsu_rdata),
    .wdata      (reg_rdata2),
    .addr       (alu_res),
    .data_size  (inst_type[1:0]),
    .is_mem_sign(inst_type[2]),

    .lsu_respValid(io_lsu_respValid),

    .lsu_reqValid (io_lsu_reqValid),
    .lsu_wdata    (io_lsu_wdata),
    .lsu_rdata    (lsu_rdata),
    .lsu_wmask    (io_lsu_wmask));

  assign pc_inc  = pc + 4;
  assign reg_wen = inst_type[3];
  
  always_comb begin
    case (inst_type)
      INST_JUMP:    reg_wdata = pc_inc;
      INST_JUMPR:   reg_wdata = pc_inc;

      INST_UPP:     reg_wdata = alu_res;
      INST_AUIPC:   reg_wdata = alu_res;
      INST_REG:     reg_wdata = alu_res;
      INST_IMM:     reg_wdata = alu_res;

      INST_CSRR:    reg_wdata = csr_rdata;
      INST_CSRRI:   reg_wdata = csr_rdata;

      INST_LOAD_B:  reg_wdata = lsu_rdata;
      INST_LOAD_H:  reg_wdata = lsu_rdata;
      INST_LOAD_W:  reg_wdata = lsu_rdata;
      INST_LOAD_BU: reg_wdata = lsu_rdata;
      INST_LOAD_HU: reg_wdata = lsu_rdata;
      
      default:      reg_wdata = 0;
    endcase
  end

  assign is_pc_jump = inst_type == INST_JUMP || inst_type == INST_JUMPR || (inst_type == INST_BRANCH && com_res);
  always_comb begin
     if (is_pc_jump) pc_next = alu_res;
     else            pc_next = pc_inc;
  end

`ifdef verilator
reg [119:0] dbg_inst_type;

always @ *
begin
    case (inst_type)
      INST_EBREAK     : dbg_inst_type = "INST_EBREAK";
      INST_CSRRW      : dbg_inst_type = "INST_CSRRW";
      INST_CSRRS      : dbg_inst_type = "INST_CSRRS";
      INST_CSRRC      : dbg_inst_type = "INST_CSRRC";
      INST_CSRRWI     : dbg_inst_type = "INST_CSRRWI";
      INST_CSRRSI     : dbg_inst_type = "INST_CSRRSI";
      INST_CSRRCI     : dbg_inst_type = "INST_CSRRCI";

      INST_LOAD_BYTE  : dbg_inst_type = "INST_LOAD_BYTE";
      INST_LOAD_HALF  : dbg_inst_type = "INST_LOAD_HALF";
      INST_LOAD_WORD  : dbg_inst_type = "INST_LOAD_WORD";
      INST_STORE_BYTE : dbg_inst_type = "INST_STORE_BYTE";
      INST_STORE_HALF : dbg_inst_type = "INST_STORE_HALF";
      INST_STORE_WORD : dbg_inst_type = "INST_STORE_WORD";

      INST_BRANCH     : dbg_inst_type = "INST_BRANCH";
      INST_IMM        : dbg_inst_type = "INST_IMM";
      INST_REG        : dbg_inst_type = "INST_REG";
      INST_UPP        : dbg_inst_type = "INST_UPP";
      INST_JUMP       : dbg_inst_type = "INST_JUMP";
      INST_JUMPR      : dbg_inst_type = "INST_JUMPR";
      INST_AUIPC      : dbg_inst_type = "INST_AUIPC";
      default         : dbg_inst_type = "INST_UNDEFINED";
    endcase
end
`endif

endmodule

