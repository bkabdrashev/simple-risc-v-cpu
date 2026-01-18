module icache (
  input  logic        clock,
  input  logic        reset,
  input  logic        wen,
  input  logic [31:0] wdata,
  input  logic [31:2] addr,
  output logic        is_hit,
  output logic [31:0] rdata);

  localparam m = 2;
  localparam n = 4;
  localparam TAG_W    = 32-m-n;
  localparam DATA_W   = 8 * (2**m);

/*
      ICACHE
  +---+-----+------+
  | 1 |TAG_W|DATA_W|
  +---+-----+------+
  | v | tag | data |
  +---+-----+------+
  |   |     |      | x16
  +---+-----+------+
*/

  typedef struct packed {
    logic              valid;
    logic [ TAG_W-1:0] tag;
    logic [DATA_W-1:0] data;
  } line_t;

  localparam LINE_N = 2**n;
  line_t lines [0:LINE_N-1];

  logic [TAG_W-1:0] tag;
  logic [    n-1:0] index;
  line_t            line;
  assign tag    = addr[   31:m+n];
  assign index  = addr[m+n-1:  m];
  assign line   = lines[index];

  generate
    for (genvar i = 0; i < LINE_N; i++) begin : gen_lines_ff
      always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
          lines[i].valid <= 1'b0;
        end
        else if (wen && i == index) begin
          lines[i] <= {1'b1, tag, wdata};
        end
      end
    end
  endgenerate


  always_comb begin
    is_hit = 1'b0;
    rdata  = 32'b0;
    if (line.valid) begin
      if (line.tag == tag) begin
        is_hit = 1'b1;
        rdata  = line.data;
      end
    end
  end

endmodule
