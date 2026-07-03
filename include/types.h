// types.h
// Core instruction representation for the RV32I pipeline simulator.
//
// DESIGN NOTE: This simulator works on *decoded* instruction objects rather
// than raw 32-bit binary encodings. That's a deliberate simplification for
// a starter project -- it lets you focus on pipeline microarchitecture
// (hazards, forwarding, prediction) instead of bit-twiddling encoders.
// A natural extension (see README "Stretch Goals") is to add a real
// encoder/decoder and load raw .bin files instead.

#pragma once
#include <cstdint>
#include <string>
#include <array>

enum class Op {
    // U-type
    LUI, AUIPC,
    // J-type
    JAL,
    // I-type jump
    JALR,
    // B-type
    BEQ, BNE, BLT, BGE, BLTU, BGEU,
    // I-type loads
    LB, LH, LW, LBU, LHU,
    // S-type stores
    SB, SH, SW,
    // I-type ALU
    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
    // R-type ALU
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
    // Misc
    NOP,
    HALT,      // synthetic "end of program" marker
    INVALID
};

// Which functional category an opcode falls into -- used heavily by the
// hazard detection unit and forwarding logic so it doesn't need a giant
// switch statement everywhere.
enum class OpClass {
    ALU_REG,    // rd = rs1 OP rs2
    ALU_IMM,    // rd = rs1 OP imm
    LOAD,       // rd = MEM[rs1+imm]
    STORE,      // MEM[rs1+imm] = rs2   (no rd)
    BRANCH,     // compare rs1,rs2, maybe redirect PC (no rd)
    JAL,        // rd = pc+4, redirect PC (no source regs)
    JALR,       // rd = pc+4, redirect PC to rs1+imm
    LUI,        // rd = imm
    AUIPC,      // rd = pc + imm
    NOP,
    HALT,
    INVALID
};

OpClass classify(Op op);
bool writesRegister(Op op);   // does this instruction have a destination reg?
bool usesRs1(Op op);
bool usesRs2(Op op);
bool isBranch(Op op);
bool isMemOp(Op op);
std::string opName(Op op);

struct Instruction {
    Op op = Op::NOP;
    int rd  = 0;
    int rs1 = 0;
    int rs2 = 0;
    int32_t imm = 0;
    uint32_t pc = 0;          // address this instruction is fetched from
    std::string text = "nop"; // original assembly text, for the diagram/trace
};

// Register ABI name -> number (x0..x31), accepts both "x5" and "t0" style.
int regNumberFromName(const std::string& name);
std::string regName(int num); // canonical "xN" form used in traces
