// Top-level single-cycle datapath
module top (clk, reset);
  input clk;
  input reset;

  // PC -> Instruction Memory
  wire [3:0] pc_addr;
  wire [7:0] instruction;

  // Decoded instruction fields
  wire [2:0] op;
  wire [1:0] rs1;
  wire [1:0] rs2;

  // Register file -> ALU
  wire [3:0] reg1_data;
  wire [3:0] reg2_data;

  // ALU outputs
  wire [3:0] alu_result;
  wire alu_zero;

  // Instantiate PC
  pc pc_inst (
    .clk(clk),
    .reset(reset),
    .pc_out(pc_addr)
  );

  // Instantiate Instruction Memory
  instr_mem imem_inst (
    .addr(pc_addr),
    .instr(instruction)
  );

  // Decode instruction into fields
  assign op  = instruction[7:5];
  assign rs1 = instruction[4:3];
  assign rs2 = instruction[2:1];
  // instruction[0] unused

  // Instantiate Register File
  // Writeback: ALU result always written back into rs1 (hardcoded destination)
  regfile regfile_inst (
    .clk(clk),
    .i_reg1(rs1),
    .i_reg2(rs2),
    .w_addr(rs1),
    .write_enable(1'b1),
    .write_data(alu_result),
    .o_reg1(reg1_data),
    .o_reg2(reg2_data)
  );

  // Instantiate ALU
  alu alu_inst (
    .op(op),
    .num1(reg1_data),
    .num2(reg2_data),
    .result(alu_result),
    .zero(alu_zero)
  );

endmodule
