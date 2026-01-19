module ifu (
  input  logic  clock,
  input  logic  reset,

  input  logic        io_respValid,
  input  logic [31:0] io_rdata,
  output logic [31:0] io_addr,
  output logic        io_reqValid,

  output logic        icache_wen,
  output logic [31:0] icache_wdata,
  output logic [31:2] icache_addr,
  input  logic        icache_hit,
  input  logic [31:0] icache_rdata,
 
  input  logic [31:0] pc,
  input  logic        reqValid,
  output logic        respValid,
  output logic [31:0] inst);

  typedef enum logic {
    IFU_IDLE, IFU_WAIT
  } ifu_state;

  ifu_state next_state;
  ifu_state curr_state;

  logic [31:0] inst_d;
  logic [31:0] inst_q;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      curr_state <= IFU_IDLE;
      inst_q     <= 32'b0;
    end else begin
      curr_state <= next_state;
      inst_q     <= inst_d;
    end
  end

  always_comb begin
    inst = inst_q;
    if (icache_hit) begin
      inst = icache_rdata;
    end
    else if (io_respValid) begin
      inst = io_rdata;
    end
  end

  assign icache_addr = pc[31:2];
  always_comb begin
    io_reqValid = 1'b0;
    respValid   = 1'b0;
    next_state  = curr_state;
    inst_d      = inst_q;
    io_addr     = pc;
    icache_wen   = 1'b0;
    icache_wdata = 32'b0;
    case (curr_state)
      IFU_IDLE: begin
        if (reqValid) begin
          if (icache_hit) begin
            respValid = 1'b1;
            inst_d    = icache_rdata;
          end
          else begin
            io_reqValid = 1'b1;
            next_state  = IFU_WAIT;
            if (io_respValid) begin
              inst_d    = io_rdata;
              respValid = 1'b1;
              icache_wen   = 1'b1;
              icache_wdata = io_rdata;
              next_state = IFU_IDLE;
            end
          end
        end
        else begin
          next_state = IFU_IDLE;
        end
      end
      IFU_WAIT: begin
        if (io_respValid) begin
          inst_d    = io_rdata;
          respValid = 1'b1;
          icache_wen   = 1'b1;
          icache_wdata = io_rdata;
          next_state = IFU_IDLE;
        end
        else begin
          next_state = IFU_WAIT;
        end
      end
    endcase
  end

`ifdef verilator
/* verilator lint_off UNUSEDSIGNAL */
reg [63:0]  dbg_ifu;

always @ * begin
  case (curr_state)
    IFU_IDLE   : dbg_ifu = "IFU_IDLE";
    IFU_WAIT   : dbg_ifu = "IFU_WAIT";
  endcase
end
/* verilator lint_on UNUSEDSIGNAL */
`endif

endmodule

