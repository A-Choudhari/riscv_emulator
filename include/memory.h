#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "types.h"

// Instruction memory: a flat array of decoded instructions, one per word.
// Separate from DataMemory so instruction fetch and data access can never
// structurally collide -- this is the "Harvard architecture" structural
// hazard elimination mentioned in the spec.
class InstructionMemory {
public:
    explicit InstructionMemory(std::vector<Instruction> program) : instrs(std::move(program)) {}

    const Instruction& fetch(uint32_t pc) const {
        size_t idx = pc / 4;
        if (idx >= instrs.size()) {
            static Instruction halt{Op::HALT, 0,0,0,0, pc, "halt"};
            return halt;
        }
        return instrs[idx];
    }

    size_t sizeWords() const { return instrs.size(); }

private:
    std::vector<Instruction> instrs;
};

// Byte-addressable data memory, little-endian, fixed size.
class DataMemory {
public:
    explicit DataMemory(size_t bytes = 1 << 16) : mem(bytes, 0) {}

    int32_t readWord(uint32_t addr) const { return readN(addr, 4, true); }
    int16_t readHalf(uint32_t addr) const { return (int16_t)readN(addr, 2, true); }
    int8_t  readByte(uint32_t addr) const { return (int8_t)readN(addr, 1, true); }
    uint16_t readHalfU(uint32_t addr) const { return (uint16_t)readN(addr, 2, false); }
    uint8_t  readByteU(uint32_t addr) const { return (uint8_t)readN(addr, 1, false); }

    void writeWord(uint32_t addr, int32_t val) { writeN(addr, (uint32_t)val, 4); }
    void writeHalf(uint32_t addr, int16_t val) { writeN(addr, (uint32_t)(uint16_t)val, 2); }
    void writeByte(uint32_t addr, int8_t val)  { writeN(addr, (uint32_t)(uint8_t)val, 1); }

private:
    std::vector<uint8_t> mem;

    void checkBounds(uint32_t addr, int n) const {
        if ((size_t)addr + n > mem.size())
            throw std::runtime_error("data memory access out of bounds at 0x" + std::to_string(addr));
    }

    int32_t readN(uint32_t addr, int n, bool signExtend) const {
        checkBounds(addr, n);
        uint32_t v = 0;
        for (int i = 0; i < n; i++) v |= (uint32_t)mem[addr + i] << (8 * i);
        if (signExtend && n < 4) {
            uint32_t signBit = 1u << (8 * n - 1);
            if (v & signBit) v |= ~((signBit << 1) - 1);
        }
        return (int32_t)v;
    }

    void writeN(uint32_t addr, uint32_t val, int n) {
        checkBounds(addr, n);
        for (int i = 0; i < n; i++) mem[addr + i] = (uint8_t)((val >> (8 * i)) & 0xFF);
    }
};
