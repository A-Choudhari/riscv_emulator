// Testbench
module test;
  reg [3:0] addr;
  wire [7:0] instr;

  // Instantiate design under test
  instr_mem uut (
    .addr(addr),
    .instr(instr)
  );

  initial begin
    $dumpfile("dump.vcd");
    $dumpvars(0, test);

    $monitor("addr=%d instr=%d",
              addr, instr);

    // Init
    addr = 0;
	#10;
    // Test 1: read from address 1
    addr = 1;
	#10;
    // Test 2: read from address 6    
    addr = 6;
	#10;
    // Test 3: read from address 7
    addr = 7;
	#10;
    // Test 4:read from address 4
    addr = 4;
	#10;
    // Test 5:read from address 3
    addr = 3;
    #10;
    $finish;
  end
endmodule
