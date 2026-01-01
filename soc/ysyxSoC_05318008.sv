module ysyxSoC_05318008 (
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

cpu u_cpu(
  .clock(clock),
  .reset(reset),

  .io_ifu_rdata(io_ifu_rdata),
  .io_ifu_respValid(io_ifu_respValid),
  .io_ifu_reqValid(io_ifu_reqValid),
  .io_ifu_addr(io_ifu_addr),

  .io_lsu_respValid(io_lsu_respValid),
  .io_lsu_rdata(io_lsu_rdata),
  .io_lsu_reqValid(io_lsu_reqValid),
  .io_lsu_addr(io_lsu_addr),
  .io_lsu_size(io_lsu_size),
  .io_lsu_wen(io_lsu_wen),
  .io_lsu_wdata(io_lsu_wdata),
  .io_lsu_wmask(io_lsu_wmask),
   .regs(/* unused */),
   .pc(/* unused */),
   .ebreak(/* unused */),
   .is_done_instruction(/* unused */));

endmodule;


