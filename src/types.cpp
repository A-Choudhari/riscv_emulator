#include "types.h"
#include <unordered_map>
#include <stdexcept>

OpClass classify(Op op) {
    switch (op) {
        case Op::ADD: case Op::SUB: case Op::SLL: case Op::SLT:
        case Op::SLTU: case Op::XOR: case Op::SRL: case Op::SRA:
        case Op::OR: case Op::AND:
            return OpClass::ALU_REG;
        case Op::ADDI: case Op::SLTI: case Op::SLTIU: case Op::XORI:
        case Op::ORI: case Op::ANDI: case Op::SLLI: case Op::SRLI:
        case Op::SRAI:
            return OpClass::ALU_IMM;
        case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU:
            return OpClass::LOAD;
        case Op::SB: case Op::SH: case Op::SW:
            return OpClass::STORE;
        case Op::BEQ: case Op::BNE: case Op::BLT: case Op::BGE:
        case Op::BLTU: case Op::BGEU:
            return OpClass::BRANCH;
        case Op::JAL:  return OpClass::JAL;
        case Op::JALR: return OpClass::JALR;
        case Op::LUI:  return OpClass::LUI;
        case Op::AUIPC: return OpClass::AUIPC;
        case Op::AMOSWAP_W: return OpClass::AMO;
        case Op::NOP:  return OpClass::NOP;
        case Op::HALT: return OpClass::HALT;
        default: return OpClass::INVALID;
    }
}

bool writesRegister(Op op) {
    OpClass c = classify(op);
    switch (c) {
        case OpClass::ALU_REG: case OpClass::ALU_IMM: case OpClass::LOAD:
        case OpClass::JAL: case OpClass::JALR: case OpClass::LUI:
        case OpClass::AUIPC: case OpClass::AMO:
            return true;
        default: return false;
    }
}

bool usesRs1(Op op) {
    OpClass c = classify(op);
    switch (c) {
        case OpClass::ALU_REG: case OpClass::ALU_IMM: case OpClass::LOAD:
        case OpClass::STORE: case OpClass::BRANCH: case OpClass::JALR:
        case OpClass::AMO:
            return true;
        default: return false;
    }
}

bool usesRs2(Op op) {
    OpClass c = classify(op);
    switch (c) {
        case OpClass::ALU_REG: case OpClass::STORE: case OpClass::BRANCH:
        case OpClass::AMO:
            return true;
        default: return false;
    }
}

bool isBranch(Op op) { return classify(op) == OpClass::BRANCH; }
bool isMemOp(Op op)  { auto c = classify(op); return c == OpClass::LOAD || c == OpClass::STORE || c == OpClass::AMO; }

std::string opName(Op op) {
    static const std::unordered_map<Op, std::string> names = {
        {Op::LUI,"lui"},{Op::AUIPC,"auipc"},{Op::JAL,"jal"},{Op::JALR,"jalr"},
        {Op::BEQ,"beq"},{Op::BNE,"bne"},{Op::BLT,"blt"},{Op::BGE,"bge"},
        {Op::BLTU,"bltu"},{Op::BGEU,"bgeu"},
        {Op::LB,"lb"},{Op::LH,"lh"},{Op::LW,"lw"},{Op::LBU,"lbu"},{Op::LHU,"lhu"},
        {Op::SB,"sb"},{Op::SH,"sh"},{Op::SW,"sw"},
        {Op::ADDI,"addi"},{Op::SLTI,"slti"},{Op::SLTIU,"sltiu"},{Op::XORI,"xori"},
        {Op::ORI,"ori"},{Op::ANDI,"andi"},{Op::SLLI,"slli"},{Op::SRLI,"srli"},{Op::SRAI,"srai"},
        {Op::ADD,"add"},{Op::SUB,"sub"},{Op::SLL,"sll"},{Op::SLT,"slt"},{Op::SLTU,"sltu"},
        {Op::XOR,"xor"},{Op::SRL,"srl"},{Op::SRA,"sra"},{Op::OR,"or"},{Op::AND,"and"},
        {Op::AMOSWAP_W,"amoswap.w"},
        {Op::NOP,"nop"},{Op::HALT,"halt"},{Op::INVALID,"???"}
    };
    auto it = names.find(op);
    return it == names.end() ? "???" : it->second;
}

static const std::unordered_map<std::string,int>& abiMap() {
    static const std::unordered_map<std::string,int> m = {
        {"zero",0},{"ra",1},{"sp",2},{"gp",3},{"tp",4},
        {"t0",5},{"t1",6},{"t2",7},
        {"s0",8},{"fp",8},{"s1",9},
        {"a0",10},{"a1",11},{"a2",12},{"a3",13},{"a4",14},{"a5",15},{"a6",16},{"a7",17},
        {"s2",18},{"s3",19},{"s4",20},{"s5",21},{"s6",22},{"s7",23},{"s8",24},{"s9",25},{"s10",26},{"s11",27},
        {"t3",28},{"t4",29},{"t5",30},{"t6",31}
    };
    return m;
}

int regNumberFromName(const std::string& name) {
    if (name.empty()) throw std::runtime_error("empty register name");
    if (name[0] == 'x' && name.size() > 1) {
        bool allDigits = true;
        for (size_t i = 1; i < name.size(); i++) if (!isdigit((unsigned char)name[i])) allDigits = false;
        if (allDigits) {
            int n = std::stoi(name.substr(1));
            if (n < 0 || n > 31) throw std::runtime_error("register out of range: " + name);
            return n;
        }
    }
    auto& m = abiMap();
    auto it = m.find(name);
    if (it != m.end()) return it->second;
    throw std::runtime_error("unknown register name: " + name);
}

std::string regName(int num) {
    return "x" + std::to_string(num);
}
