// Testbench
module test;
  reg clk;
  reg reset;
  wire [3:0] pc_out;

  // Instantiate design under test
  pc uut (
    .clk(clk),
    .reset(reset),
    .pc_out(pc_out)
  );

  // Clock generation
  always #5 clk = ~clk;

  initial begin
    $dumpfile("dump.vcd");
    $dumpvars(0, test);
    $monitor("time=%0t clk=%b reset=%b pc_out=%d",
              $time, clk, reset, pc_out);

    // Init: hold in reset
    clk = 0;
    reset = 1;

    @(negedge clk);  // reset still held -- confirm pc_out = 0
    @(negedge clk);  // reset still held -- confirm pc_out = 0

    reset = 0;       // release reset -- counting should begin next edge

    @(negedge clk);  // expect pc_out = 1
    @(negedge clk);  // expect pc_out = 2
    @(negedge clk);  // expect pc_out = 3
    @(negedge clk);  // expect pc_out = 4

    $finish;
  end
endmodule
