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

bool Simulator::step(uint64_t cycleNum) {]
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
    redirectEx_ = false;
    redirectTargetEx_ = 0;
    redirectId_ = false;
    redirectTargetId_ = 0;
    bool isStall = false;
    if (idex_.valid && (classify(idex_.instr.op) == OpClass::LOAD) && (idex_.instr.rd != 0)) {
        bool usesRs1InId = usesRs1(ifid_.instr.op) && (ifid_.instr.rs1 == idex_.instr.rd);
        bool usesRs2InId = usesRs2(ifid_.instr.op) && (ifid_.instr.rs2 == idex_.instr.rd);
        
        if (ifid_.valid && (usesRs1InId || usesRs2InId)) {
            isStall = true;
            stats_.stalls.loadUseStalls++;
        }
    }
    IF_ID_Latch nextIfId;
    ID_EX_Latch nextIdEx;
    EX_MEM_Latch nextExMem;
    MEM_WB_Latch nextMemWb;
    // 1. WB Stage (updates register file, reads current memwb_ latch)
    stageWB();
    // 2. MEM Stage (reads current exmem_, writes nextMemWb)
    stageMEM(exmem_, nextMemWb);
    // 3. EX Stage (reads current idex_, writes nextExMem)
    stageEX(idex_, nextExMem);
    // 4. ID Stage (reads current ifid_, writes nextIdEx)
    stageID(ifid_, nextIdEx);
    // 5. IF Stage (writes nextIfId)
    if (!isStall) {
        stageID(ifid_, nextIdEx);
    }
    if (redirectEx_) {
        pc_ = redirectTargetEx_;
        nextIdEx.valid = false;
        nextIfId.valid = false;
        stats_.stalls.controlHazardBubbles += 2;
    } else if (redirectId_) {
        pc_ = redirectTargetId_;
        nextIfId.valid = false;
        stats_.stalls.controlHazardBubbles += 1;
    }
    
    if (memwb_.valid) {
        stats_.instructionsRetired++;
        logStage(cycleNum, memwb_.pc, memwb_.instr.text, "WB");
    }
    if (exmem_.valid) logStage(cycleNum, exmem_.pc, exmem_.instr.text, "MEM");
    if (idex_.valid)  logStage(cycleNum, idex_.pc,  idex_.instr.text,  "EX");

    if (ifid_.valid) {
        if (isStall) {
            logStage(cycleNum, ifid_.pc, ifid_.instr.text, "STALL");
        } else {
            logStage(cycleNum, ifid_.pc, ifid_.instr.text, "ID");
        }
    }

    // Log instruction fetch (only if we actually fetched a valid instruction and did not stall)
    if (nextIfId.valid && !isStall) {
        logStage(cycleNum, nextIfId.pc, nextIfId.instr.text, "IF");
    }
    if (!isStall) {
        ifid_ = nextIfId;
    }
    idex_ = nextIdEx;
    exmem_ = nextExMem;
    memwb_ = nextMemWb;

    bool pipelineActive = ifid_.valid || idex_.valid || exmem_.valid || memwb_.valid;
    return !(halted_ && !pipelineActive);
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

    bool isStall = false;
    if (idex_.valid && classify(idex_.instr.op) == OpClass::LOAD && idex_.instr.rd != 0) {
        bool usesRs1InId = usesRs1(cur.instr.op) && (cur.instr.rs1 == idex_.instr.rd);
        bool usesRs2InId = usesRs2(cur.instr.op) && (cur.instr.rs2 == idex_.instr.rd);
        if (usesRs1InId || usesRs2InId) {
            isStall = true;
        }
    }
    if (isStall) {
        next.valid = false;
        stats_.stalls.loadUseStalls++;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;
    next.rs1val = regs_.read(cur.instr.rs1);
    next.rs2val = regs_.read(cur.instr.rs2);
    next.predictedTaken = false; // Default prediction
    if (cur.instr.op == Op::JAL) {
        redirectId_ = true;
        redirectTargetId_ = cur.pc + cur.instr.imm;
        next.predictedTaken = true; // Mark as taken
        return;
    }

    if (isBranch(cur.instr.op)) {
        bool predictTaken = predictor_.predict(cur.pc);
        next.predictedTaken = predictTaken;
        if (predictTaken) {
            redirectId_ = true;
            redirectTargetId_ = cur.pc + cur.instr.imm;
        }
    }
}

void Simulator::stageIF(IF_ID_Latch& next) {
    // 1. Fetch instruction from imem_ at pc_
    if (halted_) return;
    const Instruction& fetched = imem_.fetch(pc_);
    // 2. Halt handling
    if (fetched.op == Op::HALT) {
        halted_ = true;
        next.valid = false;
        return;
    }
    // 3. Populate next latch
    next.instr = fetched;
    next.valid = true;
    next.pc = pc_;
    // 4. Advance PC sequentially
    pc_ += 4;
}

int32_t Simulator::forwardOperand(int regNum, int32_t rawVal,
                                   const EX_MEM_Latch& exmem, const MEM_WB_Latch& memwb,
                                   bool& fromEx, bool& fromMem) {
    fromEx = fromMem = false;
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
