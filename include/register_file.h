#pragma once
#include <array>
#include <cstdint>

class RegisterFile {
public:
    RegisterFile() { regs.fill(0); }

    int32_t read(int idx) const {
        return idx == 0 ? 0 : regs[idx];
    }

    void write(int idx, int32_t value) {
        if (idx != 0) regs[idx] = value; // x0 is always zero
    }

    const std::array<int32_t, 32>& raw() const { return regs; }

private:
    std::array<int32_t, 32> regs;
};
