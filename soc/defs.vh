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
localparam ALU_OP_ANDN = 4'b1111; 
localparam ALU_OP_LHS  = 4'b1010; 
localparam ALU_OP_RHS  = 4'b1011; 
localparam ALU_OP_SUB  = 4'b1000; 
localparam ALU_OP_SRA  = 4'b1101; 

localparam COM_OP_ONE = 3'b010; 
localparam COM_OP_EQ  = 3'b000; 
localparam COM_OP_NE  = 3'b001; 
localparam COM_OP_LT  = 3'b100; 
localparam COM_OP_GE  = 3'b101; 
localparam COM_OP_LTU = 3'b110; 
localparam COM_OP_GEU = 3'b111; 

// 0-2 define the inst base type
// 3   define reg_wen
// 4   define lsu on 
// 5   define system
localparam INST_TYPE_END   = 5;
localparam INST_UNDEFINED  = 6'b000000;

localparam INST_SYSTEM     = 3'b101;
localparam INST_EBREAK     = 6'b101000;
localparam INST_CSR        = 6'b101001;
localparam INST_CSRI       = 6'b101101;

localparam INST_LOAD    = 3'b011;
localparam INST_LOAD_B  = 6'b011000;
localparam INST_LOAD_H  = 6'b011001;
localparam INST_LOAD_W  = 6'b011010;
localparam INST_LOAD_BU = 6'b011100;
localparam INST_LOAD_HU = 6'b011101;
localparam INST_STORE   = 3'b010;
localparam INST_STORE_B = 6'b010000;
localparam INST_STORE_H = 6'b010001;
localparam INST_STORE_W = 6'b010010;

localparam INST_BRANCH     = 6'b000101;
localparam INST_JUMPR      = 6'b001001;
localparam INST_JUMP       = 6'b001101;
localparam INST_REG        = 6'b001000;
localparam INST_IMM        = 6'b001010;
localparam INST_AUIPC      = 6'b001100;
localparam INST_UPP        = 6'b001110;

localparam FUNCT3_SR          = 3'b101;
localparam FUNCT3_ADD         = 3'b000;

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
