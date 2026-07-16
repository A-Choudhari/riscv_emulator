// Design
// ALU
module alu (op,
  num1, num2, result);
  input     [3:0] op;
  input     [3:0] num1;
  input     [3:0] num2;
  output	reg [3:0] result;
  output	reg zero;

  wire ADD = 3'b000;
  wire SUBTRACT = 3'b001;
  wire AND = 3'b010;
  wire OR = 3'b011;
  wire XOR = 3'b100;
  wire SLT = 3'b101;

  always @(*)
    begin
    case(op)
      ADD:
        result <= num1 + num2;
      SUBTRACT:
        result <= num1 - num2;
      AND:
        result <= num1 & num2;
      OR:
        result <= num1 | num2;
      XOR:
        result <= num ^ num2;
      SLT:
        result = (a < b) ? 4'b0001 : 4'b0000;
      default: result = 4'b0000;
    endcase
    zero = (result == 4'b0000);
    end   
  
endmodule
