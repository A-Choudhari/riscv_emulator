#include "assembler.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string stripComment(const std::string& line) {
    // supports '#' and "//" comments
    size_t hashPos = line.find('#');
    size_t slashPos = line.find("//");
    size_t cut = std::min(hashPos, slashPos);
    return cut == std::string::npos ? line : line.substr(0, cut);
}

std::vector<std::string> splitTokens(const std::string& s) {
    // splits on commas, parens, and whitespace but keeps "offset(reg)" apart
    std::string cleaned = s;
    for (char& c : cleaned) {
        if (c == ',' || c == '(' || c == ')') c = ' ';
    }
    std::istringstream iss(cleaned);
    std::vector<std::string> toks;
    std::string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}

int32_t parseImm(const std::string& s) {
    return (int32_t)std::stol(s, nullptr, 0); // supports decimal, 0x hex, -x
}

struct RawLine {
    std::string text;
    int lineNo;
};

} // namespace

std::vector<Instruction> Assembler::assembleFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open assembly file: " + path);
    std::stringstream buf;
    buf << in.rdbuf();
    return assembleSource(buf.str());
}

std::vector<Instruction> Assembler::assembleSource(const std::string& source) {
    std::istringstream stream(source);
    std::string rawLine;
    std::vector<RawLine> lines;
    int lineNo = 0;
    while (std::getline(stream, rawLine)) {
        lineNo++;
        std::string noComment = stripComment(rawLine);
        std::string trimmed = trim(noComment);
        if (!trimmed.empty()) lines.push_back({trimmed, lineNo});
    }

    // ---- Pass 1: strip labels, record label -> instruction address ----
    std::unordered_map<std::string, uint32_t> labels;
    std::vector<RawLine> instrLines; // one entry per real instruction, in order
    uint32_t addr = 0;
    for (auto& l : lines) {
        std::string text = l.text;
        // A line may be "label:" alone, or "label: instr ..."
        size_t colon = text.find(':');
        while (colon != std::string::npos) {
            std::string label = trim(text.substr(0, colon));
            if (label.empty() || label.find(' ') != std::string::npos) break; // not actually a label
            labels[label] = addr;
            text = trim(text.substr(colon + 1));
            colon = text.find(':');
            if (text.empty()) break;
        }
        if (!text.empty()) {
            instrLines.push_back({text, l.lineNo});
            addr += 4;
        }
    }

    // ---- Pass 2: decode each instruction line ----
    std::vector<Instruction> program;
    program.reserve(instrLines.size());
    addr = 0;
    for (auto& rl : instrLines) {
        std::istringstream iss(rl.text);
        std::string mnemonic;
        iss >> mnemonic;
        std::string rest;
        std::getline(iss, rest);
        auto toks = splitTokens(rest);
        std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);

        auto reg = [&](const std::string& s) { return regNumberFromName(s); };
        auto needLabel = [&](const std::string& name) -> uint32_t {
            auto it = labels.find(name);
            if (it == labels.end())
                throw std::runtime_error("line " + std::to_string(rl.lineNo) + ": unknown label '" + name + "'");
            return it->second;
        };

        Instruction ins;
        ins.pc = addr;
        ins.text = rl.text;

        try {
            // ---- Pseudo-instructions ----
            if (mnemonic == "nop") {
                ins.op = Op::ADDI; ins.rd = 0; ins.rs1 = 0; ins.imm = 0;
            } else if (mnemonic == "mv") {
                ins.op = Op::ADDI; ins.rd = reg(toks.at(0)); ins.rs1 = reg(toks.at(1)); ins.imm = 0;
            } else if (mnemonic == "li") {
                ins.op = Op::ADDI; ins.rd = reg(toks.at(0)); ins.rs1 = 0; ins.imm = parseImm(toks.at(1));
            } else if (mnemonic == "j") {
                ins.op = Op::JAL; ins.rd = 0;
                ins.imm = (int32_t)(needLabel(toks.at(0)) - addr);
            } else if (mnemonic == "ret") {
                ins.op = Op::JALR; ins.rd = 0; ins.rs1 = 1; ins.imm = 0;
            } else if (mnemonic == "beqz") {
                ins.op = Op::BEQ; ins.rs1 = reg(toks.at(0)); ins.rs2 = 0;
                ins.imm = (int32_t)(needLabel(toks.at(1)) - addr);
            } else if (mnemonic == "bnez") {
                ins.op = Op::BNE; ins.rs1 = reg(toks.at(0)); ins.rs2 = 0;
                ins.imm = (int32_t)(needLabel(toks.at(1)) - addr);
            }
            // ---- R-type ALU ----
            else if (mnemonic == "add" || mnemonic == "sub" || mnemonic == "sll" ||
                     mnemonic == "slt" || mnemonic == "sltu" || mnemonic == "xor" ||
                     mnemonic == "srl" || mnemonic == "sra" || mnemonic == "or" || mnemonic == "and") {
                static const std::unordered_map<std::string, Op> m = {
                    {"add",Op::ADD},{"sub",Op::SUB},{"sll",Op::SLL},{"slt",Op::SLT},{"sltu",Op::SLTU},
                    {"xor",Op::XOR},{"srl",Op::SRL},{"sra",Op::SRA},{"or",Op::OR},{"and",Op::AND}
                };
                ins.op = m.at(mnemonic);
                ins.rd = reg(toks.at(0)); ins.rs1 = reg(toks.at(1)); ins.rs2 = reg(toks.at(2));
            }
            // ---- I-type ALU ----
            else if (mnemonic == "addi" || mnemonic == "slti" || mnemonic == "sltiu" ||
                     mnemonic == "xori" || mnemonic == "ori" || mnemonic == "andi" ||
                     mnemonic == "slli" || mnemonic == "srli" || mnemonic == "srai") {
                static const std::unordered_map<std::string, Op> m = {
                    {"addi",Op::ADDI},{"slti",Op::SLTI},{"sltiu",Op::SLTIU},{"xori",Op::XORI},
                    {"ori",Op::ORI},{"andi",Op::ANDI},{"slli",Op::SLLI},{"srli",Op::SRLI},{"srai",Op::SRAI}
                };
                ins.op = m.at(mnemonic);
                ins.rd = reg(toks.at(0)); ins.rs1 = reg(toks.at(1)); ins.imm = parseImm(toks.at(2));
            }
            // ---- Loads: rd, offset(rs1) ----
            else if (mnemonic == "lb" || mnemonic == "lh" || mnemonic == "lw" ||
                     mnemonic == "lbu" || mnemonic == "lhu") {
                static const std::unordered_map<std::string, Op> m = {
                    {"lb",Op::LB},{"lh",Op::LH},{"lw",Op::LW},{"lbu",Op::LBU},{"lhu",Op::LHU}
                };
                ins.op = m.at(mnemonic);
                ins.rd = reg(toks.at(0)); ins.imm = parseImm(toks.at(1)); ins.rs1 = reg(toks.at(2));
            }
            // ---- Stores: rs2, offset(rs1) ----
            else if (mnemonic == "sb" || mnemonic == "sh" || mnemonic == "sw") {
                static const std::unordered_map<std::string, Op> m = {
                    {"sb",Op::SB},{"sh",Op::SH},{"sw",Op::SW}
                };
                ins.op = m.at(mnemonic);
                ins.rs2 = reg(toks.at(0)); ins.imm = parseImm(toks.at(1)); ins.rs1 = reg(toks.at(2));
            }
            // ---- Branches: rs1, rs2, label ----
            else if (mnemonic == "beq" || mnemonic == "bne" || mnemonic == "blt" ||
                     mnemonic == "bge" || mnemonic == "bltu" || mnemonic == "bgeu") {
                static const std::unordered_map<std::string, Op> m = {
                    {"beq",Op::BEQ},{"bne",Op::BNE},{"blt",Op::BLT},{"bge",Op::BGE},
                    {"bltu",Op::BLTU},{"bgeu",Op::BGEU}
                };
                ins.op = m.at(mnemonic);
                ins.rs1 = reg(toks.at(0)); ins.rs2 = reg(toks.at(1));
                ins.imm = (int32_t)(needLabel(toks.at(2)) - addr);
            }
            // ---- Jumps ----
            else if (mnemonic == "jal") {
                ins.op = Op::JAL;
                if (toks.size() == 1) { ins.rd = 1; ins.imm = (int32_t)(needLabel(toks.at(0)) - addr); }
                else { ins.rd = reg(toks.at(0)); ins.imm = (int32_t)(needLabel(toks.at(1)) - addr); }
            } else if (mnemonic == "jalr") {
                ins.op = Op::JALR;
                ins.rd = reg(toks.at(0)); ins.rs1 = reg(toks.at(1));
                ins.imm = toks.size() > 2 ? parseImm(toks.at(2)) : 0;
            }
            // ---- U-type ----
            else if (mnemonic == "lui") {
                ins.op = Op::LUI; ins.rd = reg(toks.at(0)); ins.imm = parseImm(toks.at(1));
            } else if (mnemonic == "auipc") {
                ins.op = Op::AUIPC; ins.rd = reg(toks.at(0)); ins.imm = parseImm(toks.at(1));
            } else {
                throw std::runtime_error("line " + std::to_string(rl.lineNo) + ": unknown mnemonic '" + mnemonic + "'");
            }
        } catch (const std::out_of_range&) {
            throw std::runtime_error("line " + std::to_string(rl.lineNo) + ": missing operand(s) for '" + mnemonic + "'");
        }

        program.push_back(ins);
        addr += 4;
    }

    return program;
}
