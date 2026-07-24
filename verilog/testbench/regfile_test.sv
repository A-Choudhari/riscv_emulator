// Testbench
module test;
  reg clk;
  reg [2:0] i_reg1;
  reg [2:0] i_reg2;
  reg [2:0] w_addr;
  reg write_enable;
  reg [3:0] write_data;
  wire [3:0] o_reg1;
  wire [3:0] o_reg2;

  // Instantiate design under test
  regfile uut (
    .clk(clk),
    .i_reg1(i_reg1),
    .i_reg2(i_reg2),
    .w_addr(w_addr),
    .write_enable(write_enable),
    .write_data(write_data),
    .o_reg1(o_reg1),
    .o_reg2(o_reg2)
  );

  // Clock generation
  always #5 clk = ~clk;

  initial begin
    $dumpfile("dump.vcd");
    $dumpvars(0, test);

    $monitor("time=%0t clk=%b w_addr=%d write_data=%d write_enable=%b i_reg1=%d i_reg2=%d o_reg1=%d o_reg2=%d",
              $time, clk, w_addr, write_data, write_enable, i_reg1, i_reg2, o_reg1, o_reg2);

    // Init
    clk = 0;
    write_enable = 0;
    w_addr = 0;
    write_data = 0;
    i_reg1 = 0;
    i_reg2 = 0;

    // Test 1: write 7 into register 3
    @(negedge clk);
    write_enable = 1;
    w_addr = 3'd3;
    write_data = 4'd7;

    // Test 2: read register 3 back on both ports, confirm it shows 7
    @(negedge clk);
    write_enable = 0;
    i_reg1 = 3'd3;
    i_reg2 = 3'd3;

    // Test 3: write 4 into register 5, while reading reg 3 at the same time
    @(negedge clk);
    write_enable = 1;
    w_addr = 3'd5;
    write_data = 4'd4;
    i_reg1 = 3'd3;

    // Test 4: read reg 3 and reg 5 simultaneously, confirm both correct
    @(negedge clk);
    write_enable = 0;
    i_reg1 = 3'd3;
    i_reg2 = 3'd5;

    // Test 5: attempt a "write" with write_enable low, confirm reg 3 unchanged
    @(negedge clk);
    write_enable = 0;
    w_addr = 3'd3;
    write_data = 4'd9;   // should NOT actually get written
    i_reg1 = 3'd3;

    @(negedge clk);
    $finish;
  end
endmodule
