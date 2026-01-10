module csr (
  input  logic               clock,
  input  logic               reset,
/* verilator lint_off UNUSEDSIGNAL */
  input  logic               wen,
/* verilator lint_on UNUSEDSIGNAL */
  input  logic               is_instret,
  input  logic               is_ebreak,
  input  logic [11:0]        addr,
/* verilator lint_off UNUSEDSIGNAL */
  input  logic [REG_W_END:0] wdata,
/* verilator lint_on UNUSEDSIGNAL */
  output logic [REG_W_END:0] rdata);

/* verilator lint_off UNUSEDPARAM */
`include "reg_defines.vh"
/* verilator lint_on UNUSEDPARAM */

  localparam MCYCLE_ADDR    = 12'hB00; 
  localparam MCYCLEH_ADDR   = 12'hB80; 
  localparam MINSTRET_ADDR  = 12'hB02; 
  localparam MINSTRETH_ADDR = 12'hB82; 
  localparam MVENDORID_ADDR = 12'hf11; 
  localparam MARCHID_ADDR   = 12'hf12; 
  localparam MISA_ADDR      = 12'h301; 

  // extensions:             MXL    ZY XWVUTSRQ PONMLKJI HGFEDCBA
  localparam MISA      = 32'b01_000000_00000000_00000000_00010000;
  localparam MVENDORID = "beka";
  localparam MARCHID   = 32'h05318008; 

  logic [63:0] mcycle,   mcycle_n;
  logic [63:0] minstret, minstret_n;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      mcycle   <= 64'h0;
      minstret <= 64'h0;
    end
    else begin
      mcycle   <= mcycle_n;
      minstret <= minstret_n;
    end
  end

  always_comb begin
    case (addr) 
      MISA_ADDR:      rdata = MISA;
      MARCHID_ADDR:   rdata = MARCHID;
      MVENDORID_ADDR: rdata = MVENDORID;
      MCYCLE_ADDR:    rdata = mcycle[31: 0];
      MCYCLEH_ADDR:   rdata = mcycle[63:32];
      MINSTRET_ADDR:  rdata = minstret[31: 0];
      MINSTRETH_ADDR: rdata = minstret[63:32];
      default:        rdata = 32'h0;
    endcase
  end

  always_comb begin
    mcycle_n   = mcycle + 1;
    minstret_n = minstret;
    if (is_ebreak) mcycle_n = mcycle;
    if (is_instret) minstret_n = minstret + 1;
  end

endmodule

