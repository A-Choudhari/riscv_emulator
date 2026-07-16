// Design
// ALU
module alu (op,
  num1, num2, result, zero);
  input     [2:0] op;
  input     [3:0] num1;
  input     [3:0] num2;
  output	reg [3:0] result;
  output	reg zero;

  localparam ADD = 3'b000;
  localparam SUBTRACT = 3'b001;
  localparam AND = 3'b010;
  localparam OR = 3'b011;
  localparam XOR = 3'b100;
  localparam SLT = 3'b101;

  always @(*)
    begin
    case(op)
      ADD:
        result = num1 + num2;
      SUBTRACT:
        result = num1 - num2;
      AND:
        result = num1 & num2;
      OR:
        result = num1 | num2;
      XOR:
        result = num1 ^ num2;
      SLT:
        result = (num1 < num2) ? 4'b0001 : 4'b0000;
      default: result <= 4'b0000;
    endcase
    zero = (result == 4'b0000);
    end   
  
endmodule
