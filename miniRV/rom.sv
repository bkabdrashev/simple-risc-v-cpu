module rom (
  input logic [31:0] addr,    

  output logic [31:0] read_data
);

  export "DPI-C" function sv_rom_read;
  function int unsigned sv_rom_read(input int unsigned sv_rom_addr);
    int unsigned result;
    mem_read(sv_rom_addr, result);
    return result;
  endfunction

  export "DPI-C" function sv_rom_write;
  function void sv_rom_write(input int unsigned sv_rom_addr, input int unsigned sv_rom_wdata, input byte sv_rom_wstrb);
    mem_write(sv_rom_addr, sv_rom_wdata, sv_rom_wstrb);
  endfunction

  import "DPI-C" context task mem_write(input int unsigned address, input int unsigned write, input byte wstrb);
  import "DPI-C" context task mem_read(input int unsigned address, output int unsigned read);
  import "DPI-C" context task mem_reset();

  always_comb begin
    mem_read(addr, read_data);
  end

endmodule


