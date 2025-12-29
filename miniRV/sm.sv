module sm (
  input  logic clock,
  input  logic reset,

  input  logic       ifu_respValid,
  input  logic       lsu_respValid,
  input  logic [3:0] inst_type,
  input  logic       top_mem_wen,

  output logic finished,
  output logic reg_wen,
  output logic pc_wen,
  output logic lsu_wen,
  output logic lsu_reqValid,
  output logic ifu_reqValid
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [2:0] state;
  logic [2:0] next;
  logic       next_finished;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      state <= STATE_START;
      finished <= 0;
    end
    else       begin
      state <= next;
      finished <= next_finished;
    end
  end

  always_comb begin
    ifu_reqValid = 0;
    lsu_reqValid = 0;
    pc_wen = 0;
    reg_wen = 0;
    lsu_wen = 0;
    next_finished = 0;
    unique case (state)
      STATE_START: begin
        if (top_mem_wen) begin
          next = STATE_STORE;
          lsu_reqValid = 1;
        end
        else begin
          ifu_reqValid = 1;
          next = STATE_FETCH;
        end

      end
      STATE_FETCH: begin
        if (ifu_respValid) begin
          pc_wen = 1;
          case (inst_type)
            INST_LOAD_BYTE: begin next = STATE_LOAD;  lsu_reqValid = 1; end
            INST_LOAD_HALF: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_LOAD_WORD: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_STORE:     begin next = STATE_STORE; lsu_reqValid = 1; lsu_wen = 1; end 
            default:        begin next = STATE_EXEC;  reg_wen = 1;      end
          endcase
        end
        else begin
          next = STATE_FETCH;
        end
      end
      STATE_LOAD: begin
        if (lsu_respValid) begin next = STATE_EXEC; reg_wen = 1; end
        else next = STATE_LOAD;
      end
      STATE_STORE: begin
        if (lsu_respValid && !top_mem_wen) begin
          ifu_reqValid = 1;
          next = STATE_FETCH;
        end
        else if (lsu_respValid && top_mem_wen) begin
          lsu_reqValid = 1;
          lsu_wen = 1;
          next_finished = 1;
          next = STATE_STORE;
        end
        else begin
          next = STATE_STORE;
        end
      end
      STATE_EXEC: begin
        ifu_reqValid = 1;
        next = STATE_FETCH;
        next_finished = 1;
      end
      default: begin
        ifu_reqValid = 1;
        next = STATE_FETCH;
      end
    endcase
  end

`ifdef verilator
/* verilator lint_off UNUSEDSIGNAL */
reg [79:0] dbg_cpu_state;
/* verilator lint_on UNUSEDSIGNAL */

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


