// Design
// Register File
module regfile (clk, i_reg1,
  i_reg2, w_addr, write_enable, write_data, o_reg1, o_reg2);
  input      clk;
  input      [1:0]i_reg1;
  input      [1:0]i_reg2;
  input		 [1:0]w_addr;
  input		 write_enable;
  input		 [3:0]write_data;
  output     [3:0]o_reg1;
  output     [3:0]o_reg2;

  reg [3:0] file [0:3];
  assign o_reg1 = file[i_reg1];
  assign o_reg2 = file[i_reg2];
  always @(posedge clk)
  begin
    if (write_enable) begin
      file[w_addr] <= write_data;
    end
  end
  
endmodule
