// Program Counter
module pc (clk, reset, pc_out);
  input		clk;
  input		reset;
  output	reg[3:0] pc_out;
  
  
  always @(posedge clk or posedge reset)
   begin
     if(reset) begin
    	pc_out <= 0;
     end else begin
     	pc_out <= pc_out + 1; 
     end
   end
  
endmodule
