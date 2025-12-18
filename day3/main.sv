module main (
  input logic reset,
  input logic clk,
  output logic [3:0] pc,
  output logic [7:0] regs [0:3]
);
  logic is_addr;
  logic [7:0] inst;

  logic [1:0] dec_rd;
  logic [1:0] dec_rs1;
  logic [1:0] dec_rs2;
  logic [3:0] dec_imm;
  logic [7:0] extend_imm;
  logic [1:0] dec_opcode;
  logic       dec_wen;
  logic [3:0] dec_addr;

  logic [7:0] rdata1;
  logic [7:0] rdata2;
  logic [7:0] add_res;
  logic       is_ner0;

  assign extend_imm = {4'b0, dec_imm};

  pc u_pc(
    .clk(clk),
    .reset(reset),
    .in_addr(dec_addr),
    .out_addr(pc),
    .is_addr(is_addr)
  );
  rom u_rom(.addr(pc), .inst(inst));
  dec u_dec(
    .inst(inst),
    .rd(dec_rd),
    .rs1(dec_rs1),
    .rs2(dec_rs2),
    .imm(dec_imm),
    .opcode(dec_opcode),
    .wen(dec_wen),
    .addr(dec_addr)
  );
  alu u_alu(
    .opcode(dec_opcode),
    .r0(regs[0]),
    .rdata1(rdata1),
    .rdata2(rdata2),
    .imm(extend_imm),
    .rout(add_res),
    .is_ner0(is_ner0)
  );
  rf u_rf(
    .clk(clk),
    .reset(reset),
    .wen(dec_wen),
    .rd(dec_rd),
    .wdata(add_res),
    .rs1(dec_rs1),
    .rs2(dec_rs2),
    .rdata1(rdata1),
    .rdata2(rdata2),
    .regs_out(regs)
  );

  always_comb begin
    is_addr = dec_opcode[0] & dec_opcode[1] & is_ner0 & !reset;
  end

endmodule;


