#include "simulator.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <cassert>

namespace {

int32_t aluCompute(Op op, int32_t a, int32_t b) {
    switch(op) {
        case Op::SUB:
            return a - b;
        case Op::SLL:
        case Op::SLLI:
            return a << (b & 0x1F);
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

bool branchCond(Op op, int32_t a, int32_t b) {
    switch(op) {
        case Op::BEQ:
            return a == b;
        case Op::BNE:
            return a != b;
        case Op::BLT:
            return a < b;
        case Op::BGE:
            return a >= b;
        case Op::BLTU:
            return (uint32_t)a < (uint32_t)b;
        case Op::BGEU:
            return (uint32_t)a >= (uint32_t)b;
        default:
            assert(false && "branchCond called non-branch operation");
            return false;
    }
}

} // namespace

Simulator::Simulator(std::vector<Instruction> program, int numHarts, size_t dataMemBytes)
    : imem_(std::move(program)), dmem_(dataMemBytes) {
    harts_.resize(numHarts);
    for (int i = 0; i < numHarts; i++) {
        harts_[i].id = i;
        harts_[i].regs.write(10, i); // a0 (x10) is initialized to hart ID
    }
}

bool Simulator::step(uint64_t cycleNum) {
    // 1. Detect load-use / AMO stalls for each hart using current state
    for (auto& hart : harts_) {
        hart.isStall = false;
        if (hart.idex.valid && (classify(hart.idex.instr.op) == OpClass::LOAD || hart.idex.instr.op == Op::AMOSWAP_W) && (hart.idex.instr.rd != 0)) {
            bool usesRs1InId = usesRs1(hart.ifid.instr.op) && (hart.ifid.instr.rs1 == hart.idex.instr.rd);
            bool usesRs2InId = usesRs2(hart.ifid.instr.op) && (hart.ifid.instr.rs2 == hart.idex.instr.rd);
            if (hart.ifid.valid && (usesRs1InId || usesRs2InId)) {
                hart.isStall = true;
                hart.stats.stalls.loadUseStalls++;
            }
        }
    }

    // Temporary storage for next latches
    std::vector<IF_ID_Latch> nextIfId(harts_.size());
    std::vector<ID_EX_Latch> nextIdEx(harts_.size());
    std::vector<EX_MEM_Latch> nextExMem(harts_.size());
    std::vector<MEM_WB_Latch> nextMemWb(harts_.size());

    // 2. WB Stage for each hart
    for (auto& hart : harts_) {
        stageWB(hart);
    }

    // 3. MEM Stage for each hart (shared memory access)
    for (size_t i = 0; i < harts_.size(); i++) {
        stageMEM(harts_[i], harts_[i].exmem, nextMemWb[i]);
    }

    // 4. EX Stage for each hart
    for (size_t i = 0; i < harts_.size(); i++) {
        stageEX(harts_[i], harts_[i].idex, nextExMem[i]);
    }

    // 5. ID Stage for each hart
    for (size_t i = 0; i < harts_.size(); i++) {
        stageID(harts_[i], harts_[i].ifid, nextIdEx[i]);
        if (harts_[i].isStall) {
            nextIdEx[i].valid = false;
        }
    }

    // 6. IF Stage for each hart (if not stalled)
    for (size_t i = 0; i < harts_.size(); i++) {
        if (!harts_[i].isStall) {
            stageIF(harts_[i], nextIfId[i]);
        }
    }

    // 7. Resolve redirects and update PC for each hart
    for (size_t i = 0; i < harts_.size(); i++) {
        auto& hart = harts_[i];
        if (hart.redirectEx) {
            hart.pc = hart.redirectTargetEx;
            nextIdEx[i].valid = false;
            nextIfId[i].valid = false;
            hart.stats.stalls.controlHazardBubbles += 2;
            hart.redirectEx = false;
        } else if (hart.redirectId) {
            hart.pc = hart.redirectTargetId;
            nextIfId[i].valid = false;
            hart.stats.stalls.controlHazardBubbles += 1;
            hart.redirectId = false;
        }
    }

    // 8. Log trace events for active stages of each hart
    for (size_t i = 0; i < harts_.size(); i++) {
        auto& hart = harts_[i];
        if (hart.memwb.valid) {
            hart.stats.instructionsRetired++;
            logStage(cycleNum, hart.id, hart.memwb.pc, hart.memwb.instr.text, "WB");
        }
        if (hart.exmem.valid) logStage(cycleNum, hart.id, hart.exmem.pc, hart.exmem.instr.text, "MEM");
        if (hart.idex.valid)  logStage(cycleNum, hart.id, hart.idex.pc,  hart.idex.instr.text,  "EX");

        if (hart.ifid.valid) {
            if (hart.isStall) {
                logStage(cycleNum, hart.id, hart.ifid.pc, hart.ifid.instr.text, "STALL");
            } else {
                logStage(cycleNum, hart.id, hart.ifid.pc, hart.ifid.instr.text, "ID");
            }
        }
        if (nextIfId[i].valid && !hart.isStall) {
            logStage(cycleNum, hart.id, nextIfId[i].pc, nextIfId[i].instr.text, "IF");
        }
    }

    // 9. Commit latches
    for (size_t i = 0; i < harts_.size(); i++) {
        auto& hart = harts_[i];
        if (!hart.isStall) {
            hart.ifid = nextIfId[i];
        }
        hart.idex = nextIdEx[i];
        hart.exmem = nextExMem[i];
        hart.memwb = nextMemWb[i];
    }

    // 10. Check if any hart is still active
    bool anyActive = false;
    for (auto& hart : harts_) {
        bool pipelineActive = hart.ifid.valid || hart.idex.valid || hart.exmem.valid || hart.memwb.valid;
        if (!(hart.halted && !pipelineActive)) {
            anyActive = true;
            break;
        }
    }
    return anyActive;
}

void Simulator::run(uint64_t maxCycles) {
    uint64_t cycle = 0;
    while (cycle < maxCycles) {
        cycle++;
        bool keepGoing = step(cycle);
        if (!keepGoing) { break; }
    }
    stats_.cycles = cycle;

    // Aggregate stats from all harts into stats_
    stats_.instructionsRetired = 0;
    stats_.branchesResolved = 0;
    stats_.branchMispredicts = 0;
    stats_.stalls.loadUseStalls = 0;
    stats_.stalls.controlHazardBubbles = 0;
    stats_.forwarding.exToExForwards = 0;
    stats_.forwarding.memToExForwards = 0;
    stats_.forwarding.totalAluOperandsChecked = 0;
    for (auto& hart : harts_) {
        hart.stats.cycles = cycle;
        stats_.instructionsRetired += hart.stats.instructionsRetired;
        stats_.branchesResolved += hart.stats.branchesResolved;
        stats_.branchMispredicts += hart.stats.branchMispredicts;
        stats_.stalls.loadUseStalls += hart.stats.stalls.loadUseStalls;
        stats_.stalls.controlHazardBubbles += hart.stats.stalls.controlHazardBubbles;
        stats_.forwarding.exToExForwards += hart.stats.forwarding.exToExForwards;
        stats_.forwarding.memToExForwards += hart.stats.forwarding.memToExForwards;
        stats_.forwarding.totalAluOperandsChecked += hart.stats.forwarding.totalAluOperandsChecked;
    }
}

// =========================================================================
// Pipeline Stages Stub Code -- Fill in your hardware pipeline logic here!
// =========================================================================

void Simulator::stageWB(Hart& hart) {
    // TODO: Write back values from the MEM_WB latch into hart.regs
    // Be sure to ignore writes to register x0.
    if (hart.memwb.valid) {
        // RegisterFile::write already filters writes to x0, so we can call it directly
        hart.regs.write(hart.memwb.instr.rd, hart.memwb.writebackVal);
    }
}

void Simulator::stageMEM(Hart& hart, EX_MEM_Latch& cur, MEM_WB_Latch& next) {
    if (!cur.valid) {
        next.valid = false;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;
    Op op = cur.instr.op;
    OpClass opc = classify(op);
    if (opc == OpClass::LOAD) {
        switch (op) {
            case Op::LB:  next.writebackVal = dmem_.readByte(cur.aluResult); break;
            case Op::LBU: next.writebackVal = dmem_.readByteU(cur.aluResult); break;
            case Op::LH:  next.writebackVal = dmem_.readHalf(cur.aluResult); break;
            case Op::LHU: next.writebackVal = dmem_.readHalfU(cur.aluResult); break;
            case Op::LW:  next.writebackVal = dmem_.readWord(cur.aluResult); break;
            default:      throw std::runtime_error("Unknown load opcode");
        }
    } else if (opc == OpClass::STORE) {
        switch (op) {
            case Op::SB: dmem_.writeByte(cur.aluResult, (int8_t)cur.storeVal); break;
            case Op::SH: dmem_.writeHalf(cur.aluResult, (int16_t)cur.storeVal); break;
            case Op::SW: dmem_.writeWord(cur.aluResult, cur.storeVal); break;
            default:     throw std::runtime_error("Unknown store opcode");
        }
        next.writebackVal = 0;
    } else if (op == Op::AMOSWAP_W) {
        // Atomic read-modify-write operation
        int32_t loaded = dmem_.readWord(cur.aluResult);
        dmem_.writeWord(cur.aluResult, cur.storeVal);
        next.writebackVal = loaded;
    } else {
        next.writebackVal = cur.aluResult;
    }
}

void Simulator::stageEX(Hart& hart, ID_EX_Latch& cur, EX_MEM_Latch& next) {
    if (!cur.valid) {
        next.valid = false;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;
    bool fwd1Ex = false, fwd1Mem = false;
    bool fwd2Ex = false, fwd2Mem = false;
    int32_t rs1 = forwardOperand(hart, cur.instr.rs1, cur.rs1val, hart.exmem, hart.memwb, fwd1Ex, fwd1Mem);
    int32_t rs2 = forwardOperand(hart, cur.instr.rs2, cur.rs2val, hart.exmem, hart.memwb, fwd2Ex, fwd2Mem);
    if (usesRs1(cur.instr.op)) hart.stats.forwarding.totalAluOperandsChecked++;
    if (usesRs2(cur.instr.op)) hart.stats.forwarding.totalAluOperandsChecked++;
    Op op = cur.instr.op;
    OpClass opc = classify(op);
    // Compute ALU/Address outputs
    if (opc == OpClass::ALU_REG) {
        next.aluResult = aluCompute(op, rs1, rs2);
        next.storeVal = rs2;
    } else if (opc == OpClass::ALU_IMM) {
        next.aluResult = aluCompute(op, rs1, cur.instr.imm);
        next.storeVal = 0;
    } else if (opc == OpClass::LOAD || opc == OpClass::STORE) {
        next.aluResult = rs1 + cur.instr.imm;
        next.storeVal = rs2;
    } else if (opc == OpClass::LUI) {
        next.aluResult = cur.instr.imm;
        next.storeVal = 0;
    } else if (opc == OpClass::AUIPC) {
        next.aluResult = cur.pc + cur.instr.imm;
        next.storeVal = 0;
    } else if (opc == OpClass::JAL || opc == OpClass::JALR) {
        next.aluResult = cur.pc + 4;
        next.storeVal = 0;
    } else if (opc == OpClass::AMO) {
        // AMOSWAP.W uses rs1 as address directly, and rs2 as the source register
        next.aluResult = rs1;
        next.storeVal = rs2;
    }
    // Branch execution and verification
    if (isBranch(op)) {
        bool taken = branchCond(op, rs1, rs2);
        hart.stats.branchesResolved++;
        if (taken != cur.predictedTaken) {
            hart.stats.branchMispredicts++;
            hart.redirectEx = true;
            hart.redirectTargetEx = taken ? (cur.pc + cur.instr.imm) : (cur.pc + 4);
        }
        hart.predictor.update(cur.pc, taken);
    }
    // JALR resolution (always redirects at EX)
    if (op == Op::JALR) {
        hart.redirectEx = true;
        hart.redirectTargetEx = (rs1 + cur.instr.imm) & ~1;
    }
}

void Simulator::stageID(Hart& hart, IF_ID_Latch& cur, ID_EX_Latch& next) {
    // TODO: Decode current instruction and lookup register values in hart.regs.
    // Predict branches using hart.predictor.
    // JAL must trigger an ID-stage redirect: hart.redirectId = true; hart.redirectTargetId = target;
    if (!cur.valid) {
        next.valid = false;
        return;
    }
    next.valid = true;
    next.instr = cur.instr;
    next.pc = cur.pc;
    next.rs1val = hart.regs.read(cur.instr.rs1);
    next.rs2val = hart.regs.read(cur.instr.rs2);
    next.predictedTaken = false;
    // Unconditional jumps
    if (cur.instr.op == Op::JAL) {
        hart.redirectId = true;
        hart.redirectTargetId = cur.pc + cur.instr.imm;
        next.predictedTaken = true;
    }
    // Conditional branches
    else if (isBranch(cur.instr.op)) {
        bool predictTaken = hart.predictor.predict(cur.pc);
        next.predictedTaken = predictTaken;
        if (predictTaken) {
            hart.redirectId = true;
            hart.redirectTargetId = cur.pc + cur.instr.imm;
        }
    }
}

void Simulator::stageIF(Hart& hart, IF_ID_Latch& next) {
    if (hart.halted) {
        next.valid = false;
        return;
    }
    const Instruction& fetched = imem_.fetch(hart.pc);
    if (fetched.op == Op::HALT) {
        hart.halted = true;
        next.valid = false;
        return;
    }
    next.instr = fetched;
    next.pc = hart.pc;
    next.valid = true;
    hart.pc += 4;
}

int32_t Simulator::forwardOperand(Hart& hart, int regNum, int32_t rawVal,
                                   const EX_MEM_Latch& exmem, const MEM_WB_Latch& memwb,
                                   bool& forwardedFromEx, bool& forwardedFromMem) {
    // TODO: Implement forwarding logic from EX_MEM and MEM_WB latches.
    // Increment hart.stats.forwarding.exToExForwards or memToExForwards when forwarding occurs.
    forwardedFromEx = false;
    forwardedFromMem = false;
    if (regNum == 0) return 0;
    // 1. Forward from EX/MEM stage
    if (regNum == exmem.instr.rd && exmem.valid && writesRegister(exmem.instr.op) &&
        classify(exmem.instr.op) != OpClass::LOAD && exmem.instr.op != Op::AMOSWAP_W) {
        forwardedFromEx = true;
        hart.stats.forwarding.exToExForwards++;
        return exmem.aluResult;
    }
    // 2. Forward from MEM/WB stage
    else if (regNum == memwb.instr.rd && memwb.valid && writesRegister(memwb.instr.op)) {
        forwardedFromMem = true;
        hart.stats.forwarding.memToExForwards++;
        return memwb.writebackVal;
    }
    return rawVal;
}

void Simulator::logStage(uint64_t cycle, int hartId, uint32_t pc, const std::string& text, const char* stageName) {
    trace_.push_back(TraceEvent{cycle, hartId, pc, text, stageName});
}

// -------------------------------------------------------------------------
// Reporting & Visualization
// -------------------------------------------------------------------------

void Simulator::printStats(std::ostream& os) const {
    os << "===== Multi-Hart Simulation Summary =====\n";
    os << "Harts simulated:      " << harts_.size() << "\n";
    os << "Cycles:               " << stats_.cycles << "\n";
    os << "Instructions retired: " << stats_.instructionsRetired << "\n";
    os << std::fixed << std::setprecision(3);
    os << "CPI:                   " << stats_.cpi() << "\n";
    os << "\n-- Total Stalls --\n";
    os << "Load-use stalls:      " << stats_.stalls.loadUseStalls << " cycles\n";
    os << "Control hazard bubbles:" << stats_.stalls.controlHazardBubbles << " cycles\n";
    uint64_t totalStallCycles = stats_.stalls.loadUseStalls + stats_.stalls.controlHazardBubbles;
    os << "Total stall cycles:   " << totalStallCycles
       << " (" << (stats_.cycles ? 100.0 * totalStallCycles / stats_.cycles : 0.0) << "% of all cycles)\n";
    os << "\n-- Total Branch Prediction --\n";
    os << "Branches resolved:    " << stats_.branchesResolved << "\n";
    os << "Mispredicts:          " << stats_.branchMispredicts << "\n";
    os << "Accuracy:             " << (stats_.branchAccuracy() * 100.0) << "%\n";
    os << "\n-- Total Forwarding --\n";
    os << "EX/MEM -> EX forwards: " << stats_.forwarding.exToExForwards << "\n";
    os << "MEM/WB -> EX forwards: " << stats_.forwarding.memToExForwards << "\n";
    uint64_t totalFwd = stats_.forwarding.exToExForwards + stats_.forwarding.memToExForwards;
    os << "Operands checked:      " << stats_.forwarding.totalAluOperandsChecked << "\n";
    os << "Forwarding utilization:"
       << (stats_.forwarding.totalAluOperandsChecked
               ? 100.0 * totalFwd / stats_.forwarding.totalAluOperandsChecked
               : 0.0)
       << "%\n";

    for (size_t i = 0; i < harts_.size(); i++) {
        auto& hart = harts_[i];
        os << "\n===============================\n";
        os << "--- Hart " << i << " Detailed Stats ---\n";
        os << "===============================\n";
        os << "Instructions retired: " << hart.stats.instructionsRetired << "\n";
        os << "CPI:                   " << (hart.stats.instructionsRetired ? (double)stats_.cycles / hart.stats.instructionsRetired : 0.0) << "\n";
        os << "\n-- Stalls --\n";
        os << "Load-use stalls:      " << hart.stats.stalls.loadUseStalls << " cycles\n";
        os << "Control hazard bubbles:" << hart.stats.stalls.controlHazardBubbles << " cycles\n";
        uint64_t hartStalls = hart.stats.stalls.loadUseStalls + hart.stats.stalls.controlHazardBubbles;
        os << "Total stall cycles:   " << hartStalls
           << " (" << (stats_.cycles ? 100.0 * hartStalls / stats_.cycles : 0.0) << "% of all cycles)\n";
        os << "\n-- Branch Prediction --\n";
        os << "Branches resolved:    " << hart.stats.branchesResolved << "\n";
        os << "Mispredicts:          " << hart.stats.branchMispredicts << "\n";
        os << "Accuracy:             " << (hart.stats.branchAccuracy() * 100.0) << "%\n";
        os << "\n-- Forwarding --\n";
        os << "EX/MEM -> EX forwards: " << hart.stats.forwarding.exToExForwards << "\n";
        os << "MEM/WB -> EX forwards: " << hart.stats.forwarding.memToExForwards << "\n";
        uint64_t hartFwd = hart.stats.forwarding.exToExForwards + hart.stats.forwarding.memToExForwards;
        os << "Operands checked:      " << hart.stats.forwarding.totalAluOperandsChecked << "\n";
        os << "Forwarding utilization:"
           << (hart.stats.forwarding.totalAluOperandsChecked
                   ? 100.0 * hartFwd / hart.stats.forwarding.totalAluOperandsChecked
                   : 0.0)
           << "%\n";
    }
}

void Simulator::printDiagram(std::ostream& os, uint64_t maxCycles) const {
    for (size_t h = 0; h < harts_.size(); h++) {
        os << "=== Hart " << h << " Pipeline Diagram ===\n";
        std::vector<uint32_t> order;
        std::map<uint32_t, std::string> textForPc;
        std::set<uint32_t> seen;
        for (auto& ev : trace_) {
            if (ev.hartId != (int)h) continue;
            if (!seen.count(ev.instrPc)) { seen.insert(ev.instrPc); order.push_back(ev.instrPc); }
            textForPc[ev.instrPc] = ev.instrText;
        }
        std::map<uint32_t, std::map<uint64_t, std::string>> grid;
        uint64_t maxCycleSeen = 0;
        for (auto& ev : trace_) {
            if (ev.hartId != (int)h) continue;
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
        os << "\n";
    }
}

void Simulator::writeTraceCsv(const std::string& path) const {
    std::ofstream f(path);
    f << "cycle,hart,pc,instruction,stage\n";
    for (auto& ev : trace_) {
        f << ev.cycle << "," << ev.hartId << "," << ev.instrPc << ",\"" << ev.instrText << "\"," << ev.stage << "\n";
    }
}
