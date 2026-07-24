module test;
  reg clk;
  reg reset;

  // Instantiate design under test
  top inst (
    .clk(clk),
    .reset(reset)
  );

  // Clock generation
  always #5 clk = ~clk;

  initial begin
    $dumpfile("dump.vcd");
    $dumpvars(0, test);
    $monitor("time=%0t clk=%b reset=%b pc_addr=%d instruction=%d op=%d rs1=%d rs2=%d reg1_data=%d reg2_data=%d alu_result=%d",
              $time, clk, reset, inst.pc_addr, inst.instruction, inst.op, inst.rs1,inst.rs2, inst.reg1_data,inst.reg2_data,inst.alu_result);

    // Init: hold in reset
    clk = 0;
    reset = 1;

    @(negedge clk);  // reset still held -- confirm pc_out = 0
    @(negedge clk);  // reset still held -- confirm pc_out = 0

    reset = 0;       // release reset -- counting should begin next edge

    @(negedge clk);  
    @(negedge clk); 
    @(negedge clk);  
    @(negedge clk); 

    $finish;
  end
endmodule