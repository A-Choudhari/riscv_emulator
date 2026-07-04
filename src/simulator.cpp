#include "simulator.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace {

// TODO: implement the ALU. Needs to handle both register-register and
// register-immediate variants of: ADD/ADDI, SUB, AND/ANDI, OR/ORI,
// XOR/XORI, SLL/SLLI, SRL/SRLI, SRA/SRAI (shift amount = lower 5 bits of
// b), SLT/SLTI (signed compare), SLTU/SLTIU (unsigned compare -- careful
// with the int32_t -> uint32_t cast).
int32_t aluCompute(Op op, int32_t a, int32_t b) {
    // create a switch case for each operation
    switch(op) {
        case Op::SUB:
            return a - b;
        case Op::SLL:
        case Op::SLLI:
            return a << b;
        case Op::SLT:
        case Op::SLTI:
            return a < b;
        case Op::SLTU:
        case Op::SLTIU:
            return (uint32_t)a < (uint32_t)b;
        case Op::XOR:
        case Op::XORI:
            return a ^ b;
        case Op::ADD:
        case Op::ADDI:
            return a + b;
        case Op::SRL:
        case Op::SRLI:
            return (int32_t)((uint32_t)a >> (b & 0x1F));
        case Op::SRA:
        case Op::SRAI:
            return a >> (b & 0x1F);
        case Op::OR:
        case Op::ORI:
            return a | b;
        case Op::AND:
        case Op::ANDI:
            return a & b;
        default:
            assert(false && "aluCompute called on a non-ALU opcode");
            return 0;
    }
}

// TODO: implement branch condition evaluation for BEQ/BNE/BLT/BGE/BLTU/BGEU.
// (BLTU/BGEU are unsigned compares -- same cast caveat as SLTU above.)
bool branchCond(Op op, int32_t a, int32_t b) {
    switch(op) {
        case BEQ:
            return a == b;
        case BNE:
            return a != b;
        case BLT:
            return a < b;
        case BGE:
            return a >= b;
        case BLTU:
            return (uint32_t)a < (uint32_t)b;
        case BGEU:
            return (uint32_t)a >= (uint32_t)b;
        default:
            assert(false && "branchCond called non-branch operation");
            return false;
    }
}

} // namespace

Simulator::Simulator(std::vector<Instruction> program, size_t dataMemBytes)
    : imem_(std::move(program)), dmem_(dataMemBytes) {}

bool Simulator::step(uint64_t cycleNum) {
    (void)cycleNum;
    // TODO: implement the full cycle. See the extended comment on this
    // function's declaration in simulator.h for the responsibilities.
    //
    // A reasonable skeleton to fill in:
    //
    //   1. detect load-use hazard using current idex_/ifid_ (before
    //      anything below runs)
    //   2. stageWB()
    //   3. stageMEM(exmem_, nextMemWb)
    //   4. stageEX(idex_, nextExMem)
    //   5. stageID(ifid_, nextIdEx)
    //   6. stageIF(nextIfId)
    //   7. resolve redirects/squashes -> decide pc_ for next cycle
    //   8. log trace events for this cycle
    //   9. commit: ifid_/idex_/exmem_/memwb_ = next*
    //  10. update stats_ (retirement count, cycles handled by run())
    //  11. return whether the pipeline should keep running
    throw std::logic_error("TODO: Simulator::step() not implemented yet");
}

void Simulator::run(uint64_t maxCycles) {
    uint64_t cycle = 0;
    while (cycle < maxCycles) {
        cycle++;
        bool keepGoing = step(cycle);
        if (!keepGoing) { stats_.cycles = cycle; return; }
    }
    stats_.cycles = cycle;
}

void Simulator::stageWB() {
    if(memwb_.valid) {
        regs_.write(memwb_.instr.rd(), memwb_.writebackVal);
    }
}

void Simulator::stageMEM(EX_MEM_Latch& cur, MEM_WB_Latch& next) {
    if(!cur.valid) {
        next.valid = false;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;
    next.writebackVal = cur.storeVal;

    if(classify(cur.instr.op) == OpClass::LOAD) {
        switch(cur.instr.op) {
            case Op::LB:
                next.writebackVal = dmem_.readByte(cur.aluResult);
                break;
            case Op::LBU:
                next.writebackVal = dmem_.readByte(cur.aluResult);
                break;
            case Op::LH:
                next.writebackVal = dmem_.readHalf(cur.aluResult);
                break;
            case Op::LHU:
                next.writebackVal = dmem_.readHalf(cur.aluResult);
                break;
            case Op::LW:
                next.writebackVal = dmem_.readWord(cur.aluResult);
                break;
            default:
                throw runtime_error("Invalid load opcode");
            }
    } else if(classify(cur.instr.op) == OpClass::STORE) {
        switch(curinstr.op) {
            case Op::SB:
                dmem_.writeByte(cur.aluResult, (int8_t)cur.storeVal);
                break;
            case Op::SH:
                dmem_.writeHalf(cur.aluResult, (int16_t)cur.storeVal);
                break;
            case Op::SW:
                dmem_.writeWord(cur.aluResult, cur.storeVal);
                break;
            default:
                throw runtime_error("Invalid store opcode");        
        }
        next.writebackVal = 0;
    } else {
        next.writebackVal = cur.aluResult;
    }

}

void Simulator::stageEX(ID_EX_Latch& cur, EX_MEM_Latch& next) {
    (void)cur; (void)next;
    // TODO:
    //  - Determine the actual operands to use, applying forwarding via
    //    forwardOperand() (from exmem_ / memwb_ member state) instead of
    //    the raw values captured back in ID.
    //  - Compute the ALU result per instruction class (ALU_REG, ALU_IMM,
    //    LOAD/STORE address = rs1+imm, JAL/JALR link value = pc+4,
    //    LUI/AUIPC, BRANCH condition).
    //  - For BRANCH: evaluate branchCond(), compare against whatever
    //    prediction was made in ID, update the predictor, and if it was a
    //    misprediction, signal a redirect (however you've chosen to
    //    represent that) with the correct target.
    //  - For JALR: target depends on a register value, so it can only be
    //    resolved here (not in ID) -- decide how you want to handle its
    //    redirect penalty.
    //  - Populate `next` (valid, instr, pc, aluResult, storeVal, ...).
    if (!cur.valid) {
        next.valid = false;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;

    bool fromEx, fromMem;
    int32_t rs1 = forwardOperand(cur.instr.rs1, cur.rs1Val, exmem_, memwb_, &fromEx, &fromMem);
    int32_t rs2 = forwardOperand(cur.instr.rs2, cur.rs2Val, exmem_, memwb_, &fromEx, &fromMem);
    
    if (usesRs1(cur.instr.op)) stats_.forwarding.totalAluOperandsChecked++;
    if (usesRs2(cur.instr.op)) stats_.forwarding.totalAluOperandsChecked++;

    if(classify(cur.instr.op) == OpClass::ALU_REG) {
       next.aluResult = aluCompute(cur.instr.op, rs1, rs2); 
       next.storeVal = rs2;
    } else if(classify(cur.instr.op) == OpClass::ALU_IMM) {
        next.aluResult = aluCompute(cur.instr.op, rs1, cur.instr.imm);
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::LOAD) {
        next.aluResult = rs1 + cur.instr.imm;
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::STORE) {
        next.aluResult = rs1 + cur.instr.imm;
        next.storeVal = rs2;
    } else if(classify(cur.instr.op) == OpClass::LUI) {
        next.aluResult = cur.instr.imm;
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::AUIPC) {
        next.aluResult = cur.pc + cur.instr.imm;
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::JAL) {
        next.aluResult = cur.pc + 4;
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::JALR) {
        next.aluResult = cur.pc + 4;
        next.storeVal = 0;
    } else if(classify(cur.instr.op) == OpClass::BRANCH) {
        next.aluResult = branchCond(cur.instr.op, rs1, rs2);
        next.storeVal = 0;
    }

    if (cur.instr.op == Op::JALR) {
        redirectEx_ = true;
        redirectTargetEx_ = (val1 + cur.instr.imm) & ~1;
    }

    if(isBranch(cur.instr.op)) {
        bool taken = branchCond(cur.instr.op, rs1, rs2);
        bool predict = predictor_.predict(cur.pc);

        stats_.branchesResolved++;
        if(taken != predict) {
            stats_.branchMispredicts++;
            redirectEx_ = true;
            redirectTargetEx_ = taken ? (cur.pc + cur.instr.imm) : (cur.pc + 4);
        }
        predictor_.update(cur.pc, taken);
    }
}

void Simulator::stageID(IF_ID_Latch& cur, ID_EX_Latch& next) {
    (void)cur; (void)next;
    // TODO:
    //  - If this cycle is a load-use stall, next should be a bubble
    //    (leave it invalid) and cur should be re-decoded next cycle.
    //  - Otherwise, read rs1/rs2 from regs_ (note: stageWB() should run
    //    before stageID() within the same step() so a same-cycle
    //    WB-then-ID read gets the fresh value without needing a special
    //    forwarding path).
    //  - For BRANCH instructions: consult predictor_.predict(pc) and
    //    decide whether/how to redirect fetch.
    //  - For JAL: target is fully known here (pc + imm) -- redirect
    //    unconditionally.
    //  - Populate `next` (valid, instr, pc, rs1val, rs2val, ...).
    if(!cur.valid) {
        next.valid = false;
        return;
    }
    
}

void Simulator::stageIF(IF_ID_Latch& next) {
    const Instruction& fetched = imem_.fetch(pc_);
    if(fetched.op == Op::HALT) {
        halted_ = true;
        next.valid = false;
        return;
    }
    next.instr = fetched;
    next.valid = true;
    next.pc = pc_;
    pc_+=4;
}

int32_t Simulator::forwardOperand(int regNum, int32_t rawVal,
                                   const EX_MEM_Latch& exmem, const MEM_WB_Latch& memwb,
                                   bool& fromEx, bool& fromMem) {
    fromEx = fromMem = false;
    // TODO: implement EX/MEM -> EX and MEM/WB -> EX forwarding. Remember:
    //  - x0 never forwards (always reads as 0).
    //  - EX/MEM can't forward for LOADs (data isn't ready until MEM
    //    completes) -- that's the load-use hazard, handled by stalling
    //    instead, not by forwarding.
    //  - If both exmem and memwb could supply the register, the nearer
    //    one (exmem, i.e. EX/MEM) should win.
    if(regNum == 0) {
        return 0;
    }
    if(regNum == exmem.instr.rd && exmem.valid && writesRegister(exmem.instr.op) && classify(exmem.instr.op)) {
        fromEx = true;
        return exmem.aluResult;
    } else if(regNum == memwb.instr.rd && memwb.valid && writesRegister(memwb.instr.op)) {
        fromMem = true;
        return memwb.writebackVal;
    }
    return rawVal;
}

void Simulator::logStage(uint64_t cycle, uint32_t pc, const std::string& text, const char* stageName) {
    trace_.push_back(TraceEvent{cycle, pc, text, stageName});
}

// -------------------------------------------------------------------------
// Reporting -- implemented for you. These just format whatever ends up in
// stats_ / trace_, so they'll work automatically once step() populates
// those during simulation.
// -------------------------------------------------------------------------

void Simulator::printStats(std::ostream& os) const {
    os << "===== Simulation Summary =====\n";
    os << "Cycles:               " << stats_.cycles << "\n";
    os << "Instructions retired: " << stats_.instructionsRetired << "\n";
    os << std::fixed << std::setprecision(3);
    os << "CPI:                   " << stats_.cpi() << "\n";
    os << "\n-- Stalls --\n";
    os << "Load-use stalls:      " << stats_.stalls.loadUseStalls << " cycles\n";
    os << "Control hazard bubbles:" << stats_.stalls.controlHazardBubbles << " cycles\n";
    uint64_t totalStallCycles = stats_.stalls.loadUseStalls + stats_.stalls.controlHazardBubbles;
    os << "Total stall cycles:   " << totalStallCycles
       << " (" << (stats_.cycles ? 100.0 * totalStallCycles / stats_.cycles : 0.0) << "% of all cycles)\n";
    os << "\n-- Branch Prediction --\n";
    os << "Branches resolved:    " << stats_.branchesResolved << "\n";
    os << "Mispredicts:          " << stats_.branchMispredicts << "\n";
    os << "Accuracy:             " << (stats_.branchAccuracy() * 100.0) << "%\n";
    os << "\n-- Forwarding --\n";
    os << "EX/MEM -> EX forwards: " << stats_.forwarding.exToExForwards << "\n";
    os << "MEM/WB -> EX forwards: " << stats_.forwarding.memToExForwards << "\n";
    uint64_t totalFwd = stats_.forwarding.exToExForwards + stats_.forwarding.memToExForwards;
    os << "Operands checked:      " << stats_.forwarding.totalAluOperandsChecked << "\n";
    os << "Forwarding utilization:"
       << (stats_.forwarding.totalAluOperandsChecked
               ? 100.0 * totalFwd / stats_.forwarding.totalAluOperandsChecked
               : 0.0)
       << "%\n";
}

void Simulator::printDiagram(std::ostream& os, uint64_t maxCycles) const {
    std::vector<uint32_t> order;
    std::map<uint32_t, std::string> textForPc;
    std::set<uint32_t> seen;
    for (auto& ev : trace_) {
        if (!seen.count(ev.instrPc)) { seen.insert(ev.instrPc); order.push_back(ev.instrPc); }
        textForPc[ev.instrPc] = ev.instrText;
    }
    std::map<uint32_t, std::map<uint64_t, std::string>> grid;
    uint64_t maxCycleSeen = 0;
    for (auto& ev : trace_) {
        grid[ev.instrPc][ev.cycle] = ev.stage;
        maxCycleSeen = std::max(maxCycleSeen, ev.cycle);
    }
    uint64_t upto = std::min(maxCycleSeen, maxCycles);

    os << std::left << std::setw(28) << "Instruction";
    for (uint64_t c = 1; c <= upto; c++) os << std::setw(4) << c;
    os << "\n";
    for (auto pc : order) {
        std::string label = textForPc[pc];
        if (label.size() > 26) label = label.substr(0, 23) + "...";
        os << std::left << std::setw(28) << label;
        for (uint64_t c = 1; c <= upto; c++) {
            auto it = grid[pc].find(c);
            std::string s = it == grid[pc].end() ? "." : it->second;
            os << std::setw(4) << s;
        }
        os << "\n";
    }
    if (maxCycleSeen > maxCycles) {
        os << "... (truncated at " << maxCycles << " of " << maxCycleSeen << " cycles)\n";
    }
}

void Simulator::writeTraceCsv(const std::string& path) const {
    std::ofstream f(path);
    f << "cycle,pc,instruction,stage\n";
    for (auto& ev : trace_) {
        f << ev.cycle << "," << ev.instrPc << ",\"" << ev.instrText << "\"," << ev.stage << "\n";
    }
}
