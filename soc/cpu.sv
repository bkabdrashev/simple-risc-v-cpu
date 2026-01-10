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
`include "com_defines.vh"
`include "reg_defines.vh"
`include "alu_defines.vh"
`include "inst_defines.vh"
/* verilator lint_on UNUSEDPARAM */
              
/*
              +---+        +---+          +---+
start ------->|IFU|------->|IDU| -------> |LSU|
              +---+<---+   +---+          +---+
                       |      |             |
                     +---+    |             |
                     |EXU|<---+             |
                     +---+<-----------------+
*/

  logic [REG_A_END:0]     idu_rd;
  logic [REG_A_END:0]     idu_rs1;
  logic [REG_A_END:0]     idu_rs2;
  logic [INST_TYPE_END:0] idu_inst_type;
  logic [REG_W_END:0]     idu_imm;
  logic [ALU_OP_END:0]    idu_alu_op;
  logic [COM_OP_END:0]    idu_com_op;

  logic [REG_W_END:0] pc;
  logic               pc_wen;
  logic [REG_W_END:0] pc_jump;
  logic               is_pc_jump;
  logic [REG_W_END:0] pc_next;
  logic [REG_W_END:0] pc_inc;

  logic               rf_wen;

  logic [REG_W_END:0] rf_wdata;
  logic [REG_W_END:0] rf_rdata1;
  logic [REG_W_END:0] rf_rdata2;

  logic [REG_W_END:0] ifu_inst;
  /* verilator lint_off UNUSEDSIGNAL */
  logic               is_inst_retired;
  /* verilator lint_on UNUSEDSIGNAL */
  logic               ifu_respValid;
  logic               ifu_reqValid;

  logic               exu_respValid;
  logic               exu_reqValid;

  logic               is_load_or_store;
  logic               is_load;

  logic [REG_W_END:0] lsu_rdata;
  logic [REG_W_END:0] lsu_wdata;
  logic [REG_W_END:0] lsu_addr;
  logic               lsu_respValid;
  logic               lsu_reqValid;

  logic [REG_W_END:0] csr_rdata;
  logic [REG_W_END:0] csr_wdata;
  logic               csr_wen;
  logic               ebreak;

  pc u_pc(
    .clock(clock),
    .reset(reset),
    .wen  (pc_wen),
    .wdata(pc_next),
    .rdata(pc));

  ifu u_ifu(
    .clock(clock),
    .reset(reset),

    .io_respValid(io_ifu_respValid),
    .io_reqValid (io_ifu_reqValid),
    .io_addr     (io_ifu_addr),
    .io_rdata    (io_ifu_rdata),

    .pc          (pc_next),
    .respValid   (ifu_respValid),
    .reqValid    (ifu_reqValid),
    .inst        (ifu_inst));

  idu u_idu(
    .inst(ifu_inst),

    .rd       (idu_rd),
    .rs1      (idu_rs1),
    .rs2      (idu_rs2),
    .alu_op   (idu_alu_op),
    .com_op   (idu_com_op),
    .imm      (idu_imm),
    .inst_type(idu_inst_type));

  assign is_load_or_store = idu_inst_type[4];
  assign is_load          = idu_inst_type[5:3] == INST_LOAD;

  rf u_rf(
    .clock(clock),
    .reset(reset),

    .wen   (rf_wen),
    .wdata (rf_wdata),
    .rd    (idu_rd),
    .rs1   (idu_rs1),
    .rs2   (idu_rs2),
    .rdata1(rf_rdata1),
    .rdata2(rf_rdata2));

  csr u_csr(
    .clock     (clock),
    .reset     (reset),
    .wen       (csr_wen),
    .addr      (idu_imm[11:0]),
    .is_instret(exu_respValid),
    .is_ebreak (ebreak),
    .wdata     (csr_wdata),
    .rdata     (csr_rdata));

  lsu u_lsu(
    .clock(clock),
    .reset(reset),

    .reqValid     (lsu_reqValid),
    .respValid    (lsu_respValid),
    .is_read      (is_load),
    .wdata        (lsu_wdata),
    .rdata        (lsu_rdata),
    .addr         (lsu_addr),
    .data_size    (idu_inst_type[1:0]),
    .is_mem_sign  (!idu_inst_type[2]),

    .io_respValid (io_lsu_respValid),
    .io_reqValid  (io_lsu_reqValid),
    .io_wdata     (io_lsu_wdata),
    .io_rdata     (io_lsu_rdata),
    .io_addr      (io_lsu_addr),
    .io_size      (io_lsu_size),
    .io_wen       (io_lsu_wen),
    .io_wmask     (io_lsu_wmask));

  typedef enum logic [2:0] { CPU_RESET, CPU_START, CPU_STALL_IFU, CPU_STALL_LSU, CPU_EXEC, CPU_EBREAK } cpu_state;

  cpu_state next_state;
  cpu_state curr_state;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      curr_state      <= CPU_RESET;
      is_inst_retired <= 1'b0;
    end else begin
      curr_state      <= next_state;
      is_inst_retired <= exu_respValid;
    end
  end

  assign pc_wen = exu_respValid;
  assign pc_inc = pc + 4;
  always_comb begin
    pc_next = pc_inc;
    if (!exu_respValid)  pc_next = pc;
    else if (is_pc_jump) pc_next = pc_jump;
  end

  always_comb begin
    ifu_reqValid = 1'b0;
    lsu_reqValid = 1'b0;
    exu_reqValid = 1'b0;
    case (curr_state)
      CPU_RESET: begin
        next_state = CPU_START;
      end
      CPU_START: begin
        next_state   = CPU_STALL_IFU;
        ifu_reqValid = 1'b1;
      end
      CPU_STALL_IFU: begin
        next_state = CPU_STALL_IFU;
        if (ifu_respValid) begin
          if (is_load_or_store) begin
            next_state   = CPU_STALL_LSU;
            lsu_reqValid = 1'b1;
          end
          else begin
            next_state   = CPU_EXEC;
            exu_reqValid = 1'b1;
          end
        end
      end
      CPU_STALL_LSU: begin
        next_state = CPU_STALL_LSU;
        if (lsu_respValid) begin
          next_state   = CPU_EXEC;
          exu_reqValid = 1'b1;
        end
      end
      CPU_EXEC: begin
        next_state = CPU_STALL_IFU;
        ifu_reqValid = 1'b1;
        if (ebreak) begin
          ifu_reqValid = 1'b0;
          next_state = CPU_EBREAK;
        end
      end
      CPU_EBREAK: next_state = CPU_EBREAK;
      default: next_state = curr_state;
    endcase
  end

  exu u_exu(
    .reqValid (exu_reqValid),
    .respValid(exu_respValid),
    .lsu_rdata(lsu_rdata),
    .csr_rdata(csr_rdata),
    .rdata1   (rf_rdata1),
    .rdata2   (rf_rdata2),
    .pc       (pc),
    .pc_inc   (pc_inc),

    .is_pc_jump(is_pc_jump),
    .pc_jump   (pc_jump),
    .rf_wdata  (rf_wdata),
    .rf_wen    (rf_wen),
    .csr_wen   (csr_wen),
    .csr_wdata (csr_wdata),
    .lsu_wdata (lsu_wdata),
    .lsu_addr  (lsu_addr),

    .csr_imm  (idu_rs1),
    .alu_op   (idu_alu_op),
    .com_op   (idu_com_op),
    .imm      (idu_imm),
    .inst_type(idu_inst_type));

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      ebreak <= 1'b0;
    end else begin
      ebreak <= idu_inst_type == INST_EBREAK || ebreak;
    end
  end

  always_comb begin
  end

`ifdef verilator
/* verilator lint_off UNUSEDSIGNAL */
reg [119:0] dbg_inst;
always @ * begin
  case (idu_inst_type)
    INST_EBREAK   : dbg_inst = "INST_EBREAK";
    INST_CSR      : dbg_inst = "INST_CSR";
    INST_CSRI     : dbg_inst = "INST_CSRI";

    INST_LOAD_B  : dbg_inst = "INST_LOAD_B";
    INST_LOAD_H  : dbg_inst = "INST_LOAD_H";
    INST_LOAD_W  : dbg_inst = "INST_LOAD_W";
    INST_LOAD_BU : dbg_inst = "INST_LOAD_BU";
    INST_LOAD_HU : dbg_inst = "INST_LOAD_HU";
    INST_STORE_B : dbg_inst = "INST_STORE_B";
    INST_STORE_H : dbg_inst = "INST_STORE_H";
    INST_STORE_W : dbg_inst = "INST_STORE_W";

    INST_BRANCH     : dbg_inst = "INST_BRANCH";
    INST_IMM        : dbg_inst = "INST_IMM";
    INST_REG        : dbg_inst = "INST_REG";
    INST_UPP        : dbg_inst = "INST_UPP";
    INST_JUMP       : dbg_inst = "INST_JUMP";
    INST_JUMPR      : dbg_inst = "INST_JUMPR";
    INST_AUIPC      : dbg_inst = "INST_AUIPC";
    default         : dbg_inst = "INST_UNDEFINED";
  endcase
end

reg [103:0]  dbg_cpu;
always @ * begin
  case (curr_state)
    CPU_RESET:     dbg_cpu = "CPU_RESET";
    CPU_START:     dbg_cpu = "CPU_START";
    CPU_STALL_IFU: dbg_cpu = "CPU_STALL_IFU";
    CPU_STALL_LSU: dbg_cpu = "CPU_STALL_LSU";
    CPU_EXEC:      dbg_cpu = "CPU_EXEC";
    CPU_EBREAK:    dbg_cpu = "CPU_EBREAK";
    default:       dbg_cpu = "CPU_NONE";
  endcase
end
/* verilator lint_on UNUSEDSIGNAL */
`endif

endmodule

