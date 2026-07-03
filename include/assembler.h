#pragma once
#include <vector>
#include <string>
#include "types.h"

// Minimal two-pass assembler for a useful subset of RV32I plus common
// pseudo-instructions. Produces a vector<Instruction> ready to load into
// InstructionMemory. Not a full RISC-V assembler (no directives besides
// labels, no .data section, no macros) -- see README for extension ideas.
//
// Supported real instructions:
//   lui, auipc, jal, jalr,
//   beq, bne, blt, bge, bltu, bgeu,
//   lb, lh, lw, lbu, lhu, sb, sh, sw,
//   addi, slti, sltiu, xori, ori, andi, slli, srli, srai,
//   add, sub, sll, slt, sltu, xor, srl, sra, or, and
//
// Supported pseudo-instructions:
//   nop            -> addi x0, x0, 0
//   mv  rd, rs     -> addi rd, rs, 0
//   li  rd, imm    -> addi rd, x0, imm   (imm must fit in 12 signed bits)
//   j   label      -> jal  x0, label
//   ret            -> jalr x0, x1, 0
//   beqz rs, label -> beq  rs, x0, label
//   bnez rs, label -> bne  rs, x0, label
//   call is NOT supported (no linker/multi-file support in this starter)
class Assembler {
public:
    // Throws std::runtime_error with a line-numbered message on syntax errors.
    static std::vector<Instruction> assembleFile(const std::string& path);
    static std::vector<Instruction> assembleSource(const std::string& source);
};
