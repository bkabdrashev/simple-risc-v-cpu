module csr (
  input  logic        clock,
  input  logic        reset,
  input  logic        wen,
  input  logic [11:0] addr,
  input  logic [31:0] wdata,
  output logic [31:0] rdata);
/* verilator lint_off UNUSEDPARAM */
  `include "./soc/defs.vh"
/* verilator lint_on UNUSEDPARAM */
  localparam CYCLE_ADDR     = 12'hB00; 
  localparam CYCLEH_ADDR    = 12'hB80; 
  localparam MVENDORID_ADDR = 12'hf11; 
  localparam MARCHID_ADDR   = 12'hf12; 
  localparam MISA_ADDR      = 12'h301; 

  // extensions:             MXL    ZY XWVUTSRQ PONMLKJI HGFEDCBA
  localparam MISA      = 32'b01_000000_00000000_00000000_00010000;
  localparam MVENDORID = 32'h616b6562; // beka
  localparam MARCHID   = 32'h05318008; 

  logic [63:0] mcycle;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      mcycle  <= 64'h0;
    end
    else if (wen) begin
      case (addr) 
        CYCLE_ADDR:  mcycle <= {mcycle[63:32], wdata};
        CYCLEH_ADDR: mcycle <= {wdata, mcycle[31: 0]};
        default:     mcycle <= mcycle + 1;
      endcase
    end
    else begin
      mcycle <= mcycle + 1;
    end
  end

  always_comb begin
    case (addr) 
      MISA_ADDR:      rdata = MISA;
      MARCHID_ADDR:   rdata = MARCHID;
      MVENDORID_ADDR: rdata = MVENDORID;
      CYCLE_ADDR:     rdata = mcycle[31: 0];
      CYCLEH_ADDR:    rdata = mcycle[63:32];
      default:        rdata = 32'h0;
    endcase
  end

endmodule;



