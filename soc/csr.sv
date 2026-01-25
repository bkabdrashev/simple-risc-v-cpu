module csr (
  input  logic               clock,
  input  logic               reset,
  input  logic [11:0]        addr,
  output logic [REG_W_END:0] rdata);

/* verilator lint_off UNUSEDPARAM */
`include "reg_defines.vh"
/* verilator lint_on UNUSEDPARAM */

  localparam MCYCLE        = 12'hB00;
  localparam MCYCLEH        = 12'hB80;
  localparam MVENDORID     = 12'hf11;
  localparam MARCHID       = 12'hf12;

  localparam MVENDORID_VAL = "akeb";
  localparam MARCHID_VAL   = 32'h05318008; 

  logic [63:0] mcycle;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      mcycle <= 64'h0;
    end
    else begin
      mcycle <= mcycle + 1;
    end
  end

  always_comb begin
    case (addr) 
      MARCHID:   rdata = MARCHID_VAL;
      MVENDORID: rdata = MVENDORID_VAL;
      MCYCLE:    rdata = mcycle[31: 0];
      MCYCLEH:   rdata = mcycle[63:32];
      default:   rdata = 32'h0;
    endcase
  end

endmodule

