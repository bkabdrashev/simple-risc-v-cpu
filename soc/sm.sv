module sm (
  input  logic clock,
  input  logic reset,

  input  logic       ifu_respValid,
  input  logic       lsu_respValid,
  input  logic [31:0]     lsu_rdata,
  input  logic [31:0]      lsu_addr,
  input  logic [3:0] inst_type,

  output logic finished,
  output logic ebreak,
  output logic reg_wen,
  output logic pc_wen,
  output logic lsu_wen,
  output logic lsu_reqValid,
  output logic ifu_reqValid
);
/* verilator lint_off UNUSEDPARAM */
  `include "./soc/defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [2:0] state;
  logic [2:0] next;
  logic       next_finished;
  logic       next_ebreak;
  logic ifu_inflight, lsu_inflight;

  // NOTE: this piece of code handles edges cases where ifu is requested but sudden reset was done. Then inflight becomes 0 and ifu_respValid is ignored
  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      ifu_inflight <= 1'b0;
      lsu_inflight <= 1'b0;
    end else begin
      if (ifu_reqValid) ifu_inflight <= 1'b1;
      if (ifu_respValid && ifu_inflight) ifu_inflight <= 1'b0;

      if (lsu_reqValid) lsu_inflight <= 1'b1;
      if (lsu_respValid && lsu_inflight) lsu_inflight <= 1'b0;
    end
  end

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      state <= STATE_START;
      finished <= 0;
      ebreak <= 0;
    end
    else       begin
      state <= next;
      finished <= next_finished;
      ebreak <= ebreak ? 1 : next_ebreak;
    end
  end

  always_comb begin
    ifu_reqValid = 0;
    lsu_reqValid = 0;
    pc_wen = 0;
    reg_wen = 0;
    lsu_wen = 0;
    next_finished = 0;
    next_ebreak = 0;
    unique case (state)
      STATE_START: begin
        // if (counter == 10) begin
          if (reset) begin
            next = STATE_START;
          end
          else begin
            ifu_reqValid = 1;
            next = STATE_FETCH;
          end

        // end
        // else next = STATE_START;
      end
      STATE_FETCH: begin
        if (ifu_respValid && ifu_inflight) begin
          pc_wen = 1;
          case (inst_type)
            INST_LOAD_BYTE: begin next = STATE_LOAD;  lsu_reqValid = 1; end
            INST_LOAD_HALF: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_LOAD_WORD: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_STORE:     begin next = STATE_STORE; lsu_reqValid = 1; lsu_wen = 1; end 
            default:        begin next = STATE_EXEC; reg_wen = 1;       end
          endcase
        end
        else begin
          next = STATE_FETCH;
          ifu_reqValid = 1;
        end
      end
      STATE_LOAD: begin
        if (lsu_respValid && lsu_inflight) begin
          next = STATE_EXEC; reg_wen = 1;
          if (lsu_addr == 'h1000_0005 && |lsu_rdata) $display("uart lsr: %b", lsu_rdata);
        end
        else begin
          next = STATE_LOAD;
          lsu_reqValid = 1;
        end
      end
      STATE_STORE: begin
        if (lsu_respValid && lsu_inflight) begin
          ifu_reqValid = 1;
          next_finished = 1;
          next = STATE_FETCH;
        end
        else begin
          next = STATE_STORE;
          lsu_reqValid = 1;
        end
      end
      STATE_EXEC: begin
        ifu_reqValid = 1;
        next_finished = 1;
        next = STATE_FETCH;
      end
      default: begin
        ifu_reqValid = 1;
        next = STATE_FETCH;
      end
    endcase

    if (inst_type == INST_EBREAK && next == STATE_EXEC) next_ebreak = 1;
  end

`ifdef verilator
reg [79:0] dbg_cpu_state;

always @ *
begin
    case (state)
    STATE_START   : dbg_cpu_state = "START";
    STATE_FETCH   : dbg_cpu_state = "FETCH";
    STATE_LOAD    : dbg_cpu_state = "LOAD";
    STATE_STORE   : dbg_cpu_state = "STORE";
    STATE_EXEC    : dbg_cpu_state = "EXEC";
    default       : dbg_cpu_state = "UNKNOWN";
    endcase
end
`endif

endmodule


