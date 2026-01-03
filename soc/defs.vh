localparam BUS_IDLE = 1'h0;
localparam BUS_WAIT = 1'h1;

localparam STATE_START   = 3'h0;
localparam STATE_FETCH   = 3'h1;
localparam STATE_LOAD    = 3'h2;
localparam STATE_STORE   = 3'h3;
localparam STATE_EXEC    = 3'h4;

localparam INITIAL_PC = 32'h3000_0000;
localparam N_REGS     = 16;
localparam REG_END_WORD = 31;
localparam REG_END_HALF = 15;
localparam REG_END_BYTE = 7;

localparam REG_END_ID = 4;

localparam ALU_OP_ADD  = 4'b0000; 
localparam ALU_OP_SLL  = 4'b0001; 
localparam ALU_OP_SLT  = 4'b0010; 
localparam ALU_OP_SLTU = 4'b0011; 
localparam ALU_OP_XOR  = 4'b0100; 
localparam ALU_OP_SRL  = 4'b0101; 
localparam ALU_OP_OR   = 4'b0110; 
localparam ALU_OP_AND  = 4'b0111; 
localparam ALU_OP_SUB  = 4'b1000; 
localparam ALU_OP_SRA  = 4'b1101; 

localparam COM_OP_EQ  = 3'b000; 
localparam COM_OP_NE  = 3'b001; 
localparam COM_OP_LT  = 3'b100; 
localparam COM_OP_GE  = 3'b101; 
localparam COM_OP_LTU = 3'b110; 
localparam COM_OP_GEU = 3'b111; 

localparam INST_TYPE_END   = 4;

localparam INST_UNDEFINED  = 5'b00000;

localparam INST_EBREAK     = 5'b10000;
localparam INST_CSRRW      = 5'b10001;
localparam INST_CSRRS      = 5'b10010;
localparam INST_CSRRC      = 5'b10011;
localparam INST_CSRRWI     = 5'b10101;
localparam INST_CSRRSI     = 5'b10110;
localparam INST_CSRRCI     = 5'b10111;

localparam INST_LOAD_BYTE  = 5'b01100;
localparam INST_LOAD_HALF  = 5'b01101;
localparam INST_LOAD_WORD  = 5'b01110;
localparam INST_STORE_BYTE = 5'b01000;
localparam INST_STORE_HALF = 5'b01001;
localparam INST_STORE_WORD = 5'b01010;

localparam INST_BRANCH     = 5'b00011;
localparam INST_IMM        = 5'b00100;
localparam INST_REG        = 5'b00101;
localparam INST_UPP        = 5'b00110;
localparam INST_JUMP       = 5'b00111;
localparam INST_JUMPR      = 5'b01111;

localparam FUNCT3_BYTE        = 3'b000;
localparam FUNCT3_HALF        = 3'b001;
localparam FUNCT3_WORD        = 3'b010;
localparam FUNCT3_BYTE_UNSIGN = 3'b100;
localparam FUNCT3_HALF_UNSIGN = 3'b101;
localparam FUNCT3_SR          = 3'b101;
localparam FUNCT3_ENV         = 3'b000;
localparam FUNCT3_CSRRS       = 3'b000;

localparam OPCODE_LUI       = 7'b0110111;
localparam OPCODE_AUIPC     = 7'b0010111;
localparam OPCODE_JAL       = 7'b1101111;
localparam OPCODE_JALR      = 7'b1100111;
localparam OPCODE_BRANCH    = 7'b1100011;
localparam OPCODE_LOAD      = 7'b0000011;
localparam OPCODE_STORE     = 7'b0100011;
localparam OPCODE_CALC_IMM  = 7'b0010011;
localparam OPCODE_CALC_REG  = 7'b0110011;
localparam OPCODE_SYSTEM    = 7'b1110011;
