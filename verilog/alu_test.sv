// Testbench
module test;
  reg [2:0]op;
  wire [3:0]result;
  wire zero;
  reg [3:0]num1;
  reg [3:0]num2;
  
  // Instantiate design under test
  alu alu_test(.op(op), .num1(num1),
          .num2(num2), .result(result), .zero(zero));
          
  initial begin
    // Piece 1: waveform dump
    $dumpfile("dump.vcd");
    $dumpvars(0, test);

    // Piece 2: monitor - prints every time any watched signal changes
    $monitor("time=%0t op=%b num1=%d num2=%d result=%d zero=%b",
              $time, op, num1, num2, result, zero);

    // Piece 3: test vectors
    op = 3'b000; num1 = 4'd5; num2 = 4'd3; #10; // ADD: 5+3 = 8
    op = 3'b001; num1 = 4'd5; num2 = 4'd3; #10; // SUB: 5-3 = 2
    op = 3'b010; num1 = 4'd5; num2 = 4'd3; #10; // AND: 5&3 = 1
    op = 3'b011; num1 = 4'd5; num2 = 4'd3; #10; // OR:  5|3 = 7
    op = 3'b100; num1 = 4'd5; num2 = 4'd3; #10; // XOR: 5^3 = 6
    op = 3'b101; num1 = 4'd2; num2 = 4'd7; #10; // SLT: 2<7 -> 1
    op = 3'b001; num1 = 4'd5; num2 = 4'd5; #10; // SUB: 5-5=0 -> zero=1

    // Piece 4: finish
    #10 $finish;
  end
endmodule
