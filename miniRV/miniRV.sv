module miniRV (
  input logic reg_reset,
  input logic mem_reset,
  input logic rom_wen,
  input logic [31:0] rom_wdata,
  input logic [31:0] rom_addr,
  input logic clk,
  output logic [31:0] regs_out [0:15],
  output logic [31:0] pc,
  output logic ebreak
);

  logic [31:0] inst;
  logic [4:0] dec_rd;
  logic [4:0] dec_rs1;
  logic [4:0] dec_rs2;
  logic [31:0] dec_imm;
  logic        dec_wen;
  logic [6:0]  dec_opcode;
  logic [2:0]  dec_funct3;

  logic is_pc_jump;
  logic [31:0] pc_addr;

  logic [31:0] rdata1;
  logic [31:0] rdata2;
  logic [31:0] alu_res;

  logic [31:0] wdata;

  logic        ram_wen;
 /* verilator lint_off UNOPTFLAT */
  logic [31:0] ram_addr;
  logic [31:0] ram_wdata;
  logic [31:0] ram_rdata;
  logic [3:0]  ram_wstrb;

  pc u_pc(
    .clk(clk),
    .reset(reg_reset),
    .block_increment(rom_wen),
    .in_addr(pc_addr),
    .is_addr(is_pc_jump),
    .out_addr(pc)
  );

  ram u_ram(
    .clk(clk),
    .reset(mem_reset),
    .wen(ram_wen),
    .wdata(ram_wdata),
    .wstrb(ram_wstrb),
    .addr(ram_addr),

    .read_data(ram_rdata));

  rom u_rom(
    .addr(pc),

    .read_data(inst));

  dec u_dec(
    .inst(inst),

    .rd(dec_rd),
    .rs1(dec_rs1),
    .rs2(dec_rs2),
    .imm(dec_imm),
    .wen(dec_wen),
    .opcode(dec_opcode),
    .funct3(dec_funct3)
  );

  alu u_alu(
    .opcode(dec_opcode),
    .rdata1(rdata1),
    .rdata2(rdata2),
    .imm(dec_imm),

    .rout(alu_res)
  );

  rf u_rf(
    .clk(clk),
    .reset(reg_reset),
    .wen(dec_wen),
    .rd(dec_rd),
    .wdata(wdata),
    .rs1(dec_rs1),
    .rs2(dec_rs2),

    .rdata1(rdata1),
    .rdata2(rdata2),
    .regs_out(regs_out)
  );

  always_comb begin
    if (rom_wen) begin
      ram_wen = 1;
      ram_addr = rom_addr;
      ram_wdata = rom_wdata;
      ram_wstrb = 4'b1111;
      wdata = 0;
      pc_addr = 0;
      is_pc_jump = 0;
      ebreak = 0;
    end else begin
      if (dec_opcode == 7'b0010011) begin
        // ADDI
        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;
        wdata = alu_res;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b1100111) begin
        // JALR
        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 0;

        wdata = pc+4;
        pc_addr = (rdata1 + dec_imm) & ~3;
        is_pc_jump = 1;
        ebreak = 0;
      end else if (dec_opcode == 7'b0110011) begin
        // ADD
        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;

        wdata = alu_res;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b0110111) begin
        // LUI
        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;

        wdata = dec_imm;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b0000011 && dec_funct3 == 3'b010) begin
        // LW
        ram_wen = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;
        ram_addr = rdata1 + dec_imm;

        wdata = ram_rdata;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b0000011 && dec_funct3 == 3'b100) begin
        // LBU
        ram_wen = 0;
        ram_wdata = 0;
        ram_wstrb = 0;
        ram_addr = rdata1 + dec_imm;

        wdata = ram_rdata & 32'hff;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b0100011 && dec_funct3 == 3'b010) begin
        // SW
        ram_wen = 1;
        ram_addr = rdata1 + dec_imm;
        ram_wdata = rdata2;
        ram_wstrb = 4'b1111;

        wdata = 0;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b0100011 && dec_funct3 == 3'b000) begin
        // SB
        ram_wen = 1;
        ram_addr = rdata1 + dec_imm;
        ram_wdata = rdata2 & 32'hff;
        ram_wstrb = 4'b0001;

        wdata = 0;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end else if (dec_opcode == 7'b1110011 && dec_imm == 1) begin
        // EBREAK

        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;

        wdata = 0;
        pc_addr = 0;
        is_pc_jump = 0;

        ebreak = 1;
        // $finish;
      end else begin
        // $strobe ("DUT WARNING: not implemented %h", clk);
        // NOT IMPLEMENTED
        ram_wen = 0;
        ram_addr = 0;
        ram_wdata = 0;
        ram_wstrb = 4'b0000;

        wdata = 0;
        pc_addr = 0;
        is_pc_jump = 0;
        ebreak = 0;
      end
    end
  end

endmodule;


