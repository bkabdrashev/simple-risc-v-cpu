module lsu (
  input logic         clock,
  input logic         reset,

  input               reqValid,
  output              respValid,
  input               is_write,
  input  logic [31:0] wdata,
  input  logic [31:0] addr,
  input  logic [1:0]  data_size,
  input  logic        is_mem_sign,
  output logic [31:0] rdata,

  output logic        io_reqValid,
  input  logic        io_respValid,
  output logic [31:0] io_wdata,
  input  logic [31:0] io_rdata,
  output logic [31:0] io_addr,
  output logic [1:0]  io_size,
  output logic        io_wen,
  output logic [3:0]  io_wmask);

  localparam LSU_BYTE = 2'b00;
  localparam LSU_HALF = 2'b01;
  localparam LSU_WORD = 2'b10;
  localparam LSU_EXTA = 2'b11;

  logic [31:8] first_rdata;
  logic [31:8] first_rdata_q;
  logic [31:0] align_rdata;
  logic [1:0]  addr_offset;
  logic        mem_byte_sign;
  logic        mem_half_sign;
  logic [23:0] mem_byte_extend;
  logic [15:0] mem_half_extend;
  logic        is_misalign;
  logic        is_second_part;
  logic        is_read;

  assign is_misalign = (addr_offset != 2'b00 && data_size == LSU_WORD) ||
                       (addr_offset == 2'b11 && data_size == LSU_HALF) ;;
  assign addr_offset = addr[1:0];
  assign io_addr     = is_second_part ? {addr[31:2]+29'b1, 2'b00} : addr;
  assign io_size     = data_size;
  assign io_wen      = is_write;
  assign is_read     = ~is_write;

  always_comb begin
    case (addr_offset)
      2'b00: io_wdata =  wdata[31:0];
      2'b01: io_wdata = {wdata[23:0], wdata[31:24]};
      2'b10: io_wdata = {wdata[15:0], wdata[31:16]};
      2'b11: io_wdata = {wdata[ 7:0], wdata[31: 8]};
    endcase

    case (data_size)
      LSU_BYTE: begin
        case (addr_offset)
          2'b00: io_wmask = 4'b0001;
          2'b01: io_wmask = 4'b0010;
          2'b10: io_wmask = 4'b0100;
          2'b11: io_wmask = 4'b1000;
        endcase
      end
      LSU_HALF: begin
        if (is_second_part)
          io_wmask = 4'b0001;
        else
          case (addr_offset)
            2'b00: io_wmask = 4'b0011;
            2'b01: io_wmask = 4'b0110;
            2'b10: io_wmask = 4'b1100;
            2'b11: io_wmask = 4'b1000;
          endcase
      end
      LSU_WORD: begin
        if (is_second_part)
          case (addr_offset)
            2'b00: io_wmask = 4'b1111;
            2'b01: io_wmask = 4'b0001;
            2'b10: io_wmask = 4'b0011;
            2'b11: io_wmask = 4'b0111;
          endcase
        else
          case (addr_offset)
            2'b00: io_wmask = 4'b1111;
            2'b01: io_wmask = 4'b1110;
            2'b10: io_wmask = 4'b1100;
            2'b11: io_wmask = 4'b1000;
          endcase
      end
      LSU_EXTA: begin
        io_wmask = 4'b1111;
      end
    endcase
  end

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      first_rdata_q  <= 24'b0;
    end
    else begin
      if (is_read && io_respValid) begin
        first_rdata_q  <= io_rdata[31:8];
      end
    end
  end
  always_comb begin
    case (addr_offset)
      2'b00: align_rdata =  io_rdata[31:0];
      2'b01: align_rdata = {io_rdata[ 7:0], first_rdata[31: 8]};
      2'b10: align_rdata = {io_rdata[15:0], first_rdata[31:16]};
      2'b11: align_rdata = {io_rdata[23:0], first_rdata[31:24]};
    endcase
  end

  assign mem_byte_sign   = align_rdata[ 7] & is_mem_sign;
  assign mem_half_sign   = align_rdata[15] & is_mem_sign;
  assign mem_byte_extend = {24{mem_byte_sign}};
  assign mem_half_extend = {16{mem_half_sign}};

  always_comb begin
    case (data_size)
      LSU_BYTE: rdata = {mem_byte_extend, align_rdata[ 7:0]};
      LSU_HALF: rdata = {mem_half_extend, align_rdata[15:0]};
      LSU_WORD: rdata = align_rdata[31:0];
      LSU_EXTA: rdata = align_rdata[31:0];
    endcase
  end

  typedef enum logic [2:0] {
    LSU_IDLE, LSU_WAIT_ONE, LSU_WAIT_MIS_ONE, LSU_WAIT_MIS_TWO, LSU_WAIT_MIS_TWO_REQ
  } lsu_state;

  lsu_state next_state;
  lsu_state curr_state;
  logic     io_reqValid_q;
  logic     io_reqValid_d;
  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      curr_state <= LSU_IDLE;
      io_reqValid_q <= 1'b0;
    end else begin
      curr_state    <= next_state;
      io_reqValid_q <= io_reqValid_d;
    end
  end

  assign io_reqValid = io_reqValid_d | io_reqValid_q;
  always_comb begin
    io_reqValid_d  = 1'b0;
    respValid      = 1'b0;
    is_second_part = 1'b0;
    first_rdata    = io_rdata[31:8];
    case (curr_state)
      LSU_IDLE: begin
        if (reqValid) begin
          io_reqValid_d   = 1'b1;
          if (io_respValid) begin
            respValid      = ~is_misalign;
            next_state     = is_misalign ? LSU_WAIT_MIS_TWO_REQ : LSU_IDLE;
          end
          else begin
            next_state = is_misalign ? LSU_WAIT_MIS_ONE : LSU_WAIT_ONE;
          end
        end
        else begin
          next_state = LSU_IDLE;
        end
      end
      LSU_WAIT_ONE: begin
        if (io_respValid) begin
          next_state = LSU_IDLE;
          respValid  = 1'b1;
        end
        else begin
          next_state  = LSU_WAIT_ONE;
        end
      end
      LSU_WAIT_MIS_TWO: begin
        is_second_part = 1'b1;
        if (io_respValid) begin
          next_state  = LSU_IDLE;
          first_rdata = first_rdata_q;
          respValid   = 1'b1;
        end
        else begin
          next_state = LSU_WAIT_MIS_TWO;
        end
      end
      LSU_WAIT_MIS_TWO_REQ: begin
        is_second_part = 1'b1;
        io_reqValid_d  = 1'b1;
        if (io_respValid) begin
          next_state  = LSU_IDLE;
          first_rdata = first_rdata_q;
          respValid   = 1'b1;
        end
        else begin
          next_state = LSU_WAIT_MIS_TWO;
        end
      end
      LSU_WAIT_MIS_ONE: begin
        if (io_respValid) begin
          is_second_part = 1'b1;
          io_reqValid_d    = 1'b1;
          respValid      = 1'b0;
          next_state     = LSU_WAIT_MIS_TWO;
        end
        else begin
          next_state = LSU_WAIT_MIS_ONE;
        end
      end
      default: begin
        next_state = LSU_IDLE;
      end
    endcase
  end

`ifdef verilator
/* verilator lint_off UNUSEDSIGNAL */
reg [159:0]  dbg_lsu;

always @ * begin
  case (curr_state)
    LSU_IDLE             : dbg_lsu = "LSU_IDLE";
    LSU_WAIT_ONE         : dbg_lsu = "LSU_WAIT_ONE";
    LSU_WAIT_MIS_ONE     : dbg_lsu = "LSU_WAIT_MIS_ONE";
    LSU_WAIT_MIS_TWO     : dbg_lsu = "LSU_WAIT_MIS_TWO";
    LSU_WAIT_MIS_TWO_REQ : dbg_lsu = "LSU_WAIT_MIS_TWO_REQ";
    default              : dbg_lsu = "LSU_UNDEFINED";
  endcase
end
/* verilator lint_on UNUSEDSIGNAL */
`endif
endmodule
