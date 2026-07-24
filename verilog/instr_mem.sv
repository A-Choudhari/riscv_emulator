// Instruction Memory
module instr_mem (addr, instr);
  input     [3:0] addr;
  output	[7:0] instr;

  reg [7:0] mem [0:15];
  
  initial begin
    mem[0]  = 8'b00001100;
    mem[1]  = 8'b00110010;
    mem[2]  = 8'b01001110;
    mem[3]  = 8'b00101010;
    mem[4]  = 8'b00010000;
    mem[5]  = 8'b00100000;
    mem[6]  = 8'b01000000;
    mem[7]  = 8'b10000000;
  end
  assign instr = mem[addr];
endmodule
