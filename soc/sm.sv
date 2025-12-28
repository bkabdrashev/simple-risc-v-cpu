module sm (
  input  logic clock,
  input  logic reset,

  input  logic [2:0] in,
  input  logic       ifu_respValid,
  input  logic       lsu_respValid,
  input  logic [3:0] inst_type,

  output logic reg_wen,
  output logic pc_wen,
  output logic ifu_reqValid,
  output logic lsu_reqValid,
  output logic finished,
  output logic [2:0] out
);
/* verilator lint_off UNUSEDPARAM */
  `include "defs.vh"
/* verilator lint_on UNUSEDPARAM */

  logic [2:0] next;

  always_ff @(posedge clock or posedge reset) begin
    if (reset) out <= STATE_FETCH;
    else       out <= next;
  end

  always_comb begin
    finished = 0;
    ifu_reqValid = 0;
    lsu_reqValid = 0;
    reg_wen = 0;
    pc_wen = 0;
    unique case (in)
      STATE_FETCH: begin
        if (ifu_respValid) begin
          pc_wen = 1;
          case (inst_type)
            INST_LOAD_BYTE: begin next = STATE_LOAD;  lsu_reqValid = 1; end
            INST_LOAD_HALF: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_LOAD_WORD: begin next = STATE_LOAD;  lsu_reqValid = 1; end 
            INST_STORE:     begin next = STATE_STORE; lsu_reqValid = 1; end 
            default:        begin next = STATE_EXEC;  reg_wen = 1;      end
          endcase
        end
        else begin
          ifu_reqValid = 1;
          next = STATE_FETCH;
        end
      end
      STATE_LOAD: begin
        if (lsu_respValid) begin next = STATE_EXEC; reg_wen = 1; end
        else next = STATE_LOAD;
      end
      STATE_STORE: begin
        if (lsu_respValid) begin
          finished = 1;
          ifu_reqValid = 1;
          next = STATE_FETCH;
        end
        else begin
          lsu_reqValid = 1;
          next = STATE_STORE;
        end
      end
      STATE_EXEC: begin
        finished = 1;
        next = STATE_FETCH;
      end
      default: begin
        next = STATE_FETCH;
      end
    endcase
  end

endmodule


