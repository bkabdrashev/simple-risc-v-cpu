module sm (
  input  logic clock,
  input  logic reset,

  input  logic       ifu_respValid,
  input  logic       lsu_respValid,
  input  logic [3:0] inst_type,

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
  logic [4:0] counter;
  logic       next_lsu_wen;
  logic       next_lsu_reqValid;
  logic       next_ifu_reqValid;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) begin
      state <= STATE_START;
      counter <= 0;
    end
    else       begin
      state <= next;
      lsu_wen      <= next_lsu_wen;
      lsu_reqValid <= next_lsu_reqValid;
      ifu_reqValid <= next_ifu_reqValid;
      counter <= counter + 1;
    end
  end

  always_comb begin
    next_ifu_reqValid = 0;
    next_lsu_reqValid = 0;
    pc_wen = 0;
    reg_wen = 0;
    next_lsu_wen = 0;
    unique case (state)
      STATE_START: begin
        if (counter == 10) begin
          next_ifu_reqValid = 1;
          next = STATE_FETCH;
        end
        else next = STATE_START;
      end
      STATE_FETCH: begin
        if (ifu_respValid) begin
          pc_wen = 1;
          case (inst_type)
            INST_LOAD_BYTE: begin next = STATE_LOAD;  next_lsu_reqValid = 1; end
            INST_LOAD_HALF: begin next = STATE_LOAD;  next_lsu_reqValid = 1; end 
            INST_LOAD_WORD: begin next = STATE_LOAD;  next_lsu_reqValid = 1; end 
            INST_STORE:     begin next = STATE_STORE; next_lsu_reqValid = 1; next_lsu_wen = 1; end 
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
        if (lsu_respValid) begin
          next = STATE_EXEC;
        end
        else begin
          next = STATE_STORE;
        end
      end
      STATE_EXEC: begin
        next_ifu_reqValid = 1;
        next = STATE_FETCH;
      end
      default: begin
        next_ifu_reqValid = 1;
        next = STATE_FETCH;
      end
    endcase
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


