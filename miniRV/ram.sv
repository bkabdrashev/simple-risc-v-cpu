module ram (
  input  logic        clk,
  input  logic        reset,
  input  logic        wen,
  input  logic [31:0] wdata,
  input  logic [3:0]  wstrb,
  input  logic [31:0] addr,    

  output logic [31:0] read_data
);
  export "DPI-C" function sv_mem_read;
  function int unsigned sv_mem_read(input int unsigned mem_addr);
    int unsigned result;
    mem_read(mem_addr, result);
    return result;
  endfunction

  export "DPI-C" function sv_mem_write;
  function void sv_mem_write(input int unsigned mem_addr, input int unsigned mem_wdata, input byte mem_wstrb);
    mem_write(mem_addr, mem_wdata, mem_wstrb);
  endfunction

  import "DPI-C" context task mem_write(input int unsigned address, input int unsigned write, input byte wstrb);
  import "DPI-C" context task mem_read(input int unsigned address, output int unsigned read);
  import "DPI-C" context task mem_reset();

  always_ff @(posedge clk or posedge reset) begin
    if (reset) begin
      mem_reset();
    end else begin
      if (wen) mem_write(addr, wdata, {4'b0, wstrb});
    end
  end

  always_comb begin
    mem_read(addr, read_data);
  end

endmodule


