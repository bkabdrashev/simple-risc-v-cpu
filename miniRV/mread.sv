module mread (
  input logic                  clock,
  input logic                  reset,
  input logic                  reqValid,
  input logic [REG_END_WORD:0] addr,    

  output logic                  busy,
  output logic                  respValid,
  output logic [REG_END_WORD:0] rdata
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */
  import "DPI-C" context function int unsigned mem_read(input int unsigned address);

  logic [1:0] counter; 
  logic [REG_END_WORD:0] addr_q;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      counter   <= '0;
      busy      <= 1'b0;
      respValid <= 1'b0;
      rdata     <= '0;
      addr_q    <= '0;
    end else begin
      respValid <= 1'b0;

      if (!busy) begin
        if (reqValid) begin
          busy    <= 1'b1;
          counter <= 2'd0;
          addr_q  <= addr;
        end
      end else begin
        if (counter == 2'd3) begin
          rdata     <= mem_read(addr_q);
          respValid <= 1'b1;
          busy      <= 1'b0;
        end else begin
          counter <= counter + 2'd1;
        end
      end
    end
  end
endmodule


