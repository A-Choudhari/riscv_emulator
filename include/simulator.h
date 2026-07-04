#pragma once
#include <vector>
#include <string>
#include <iosfwd>
#include "types.h"
#include "register_file.h"
#include "memory.h"
#include "branch_predictor.h"

// ---------------------------------------------------------------------
// Pipeline latches (the "pipeline registers" between IF/ID/EX/MEM/WB).
//
// These hold what the NEXT stage needs. The general pattern for a
// cycle-accurate simulator like this is a "two-phase" update: every
// cycle, compute a fresh "next" copy of each latch from the CURRENT
// latch contents (this is what makes all 5 stages appear to act
// simultaneously), and only overwrite the current latches at the very
// end of the cycle once every stage has read what it needed.
//
// Feel free to add fields to these as your hazard/prediction design
// needs them (e.g. you'll likely want somewhere to stash the branch
// prediction made for an instruction so EX can check it against the
// actual outcome).
// ---------------------------------------------------------------------

struct IF_ID_Latch {
    bool valid = false;
    Instruction instr;
    uint32_t pc = 0;
};

struct ID_EX_Latch {
    bool valid = false;
    Instruction instr;
    uint32_t pc = 0;
    int32_t rs1val = 0, rs2val = 0;
    bool predictedTaken = false; // branch prediction made in ID
    // TODO: add whatever else EX needs -- e.g. branch prediction info if
    // you're predicting in ID.
};

struct EX_MEM_Latch {
    bool valid = false;
    Instruction instr;
    uint32_t pc = 0;
    int32_t aluResult = 0;
    int32_t storeVal = 0;       // value to store (rs2), for STORE ops
    // TODO: add whatever else MEM/WB need -- e.g. actual branch outcome.
};

struct MEM_WB_Latch {
    bool valid = false;
    Instruction instr;
    uint32_t pc = 0;
    int32_t writebackVal = 0;
};

// One row of the cycle-by-cycle diagram: which stage (if any) each
// in-flight instruction occupied on a given cycle. Feed these into
// printDiagram()/writeTraceCsv() as you populate trace_ during step().
struct TraceEvent {
    uint64_t cycle;
    int hartId;
    uint32_t instrPc;
    std::string instrText;
    std::string stage; // e.g. "IF","ID","EX","MEM","WB","STALL","FLUSH"
};

struct StallStats {
    uint64_t loadUseStalls = 0;
    uint64_t controlHazardBubbles = 0; // cycles lost to branch/jump redirects
    uint64_t structuralStalls = 0;     // reserved; this design has none by construction
};

struct ForwardingStats {
    uint64_t exToExForwards = 0;   // EX/MEM -> EX
    uint64_t memToExForwards = 0;  // MEM/WB -> EX
    uint64_t totalAluOperandsChecked = 0;
};

struct SimulationStats {
    uint64_t cycles = 0;
    uint64_t instructionsRetired = 0;
    StallStats stalls;
    ForwardingStats forwarding;
    uint64_t branchesResolved = 0;
    uint64_t branchMispredicts = 0;

    double cpi() const {
        return instructionsRetired == 0 ? 0.0 : (double)cycles / (double)instructionsRetired;
    }
    double branchAccuracy() const {
        return branchesResolved == 0 ? 0.0 : 1.0 - (double)branchMispredicts / (double)branchesResolved;
    }
};

struct Hart {
    int id = 0;
    RegisterFile regs;
    BranchPredictor predictor;
    IF_ID_Latch ifid;
    ID_EX_Latch idex;
    EX_MEM_Latch exmem;
    MEM_WB_Latch memwb;

    uint32_t pc = 0;
    bool halted = false;

    bool redirectEx = false;
    uint32_t redirectTargetEx = 0;
    bool redirectId = false;
    uint32_t redirectTargetId = 0;
    bool isStall = false;

    SimulationStats stats;
};

class Simulator {
public:
    Simulator(std::vector<Instruction> program, int numHarts = 1, size_t dataMemBytes = 1 << 16);

    // Runs until the pipeline drains after fetching HALT, or maxCycles is hit.
    void run(uint64_t maxCycles = 2'000'000);

    const SimulationStats& stats() const { return stats_; }
    const std::vector<TraceEvent>& trace() const { return trace_; }
    const RegisterFile& registers(int hartId = 0) const { return harts_.at(hartId).regs; }
    const DataMemory& memory() const { return dmem_; }

    // Pretty-prints the classic Patterson & Hennessy colored-grid pipeline
    // diagram as ASCII to stdout (stage letters per cycle per instruction).
    // Implemented for you -- just needs trace_ to be populated by step().
    void printDiagram(std::ostream& os, uint64_t maxCycles = 60) const;
    void printStats(std::ostream& os) const;

    // Dumps trace_ as CSV for viz/pipeline_diagram.py. Implemented for you.
    void writeTraceCsv(const std::string& path) const;

private:
    InstructionMemory imem_;
    DataMemory dmem_;
    SimulationStats stats_; // Aggregated stats
    std::vector<TraceEvent> trace_;

    std::vector<Hart> harts_;

    // One simulated clock edge. Return false once the pipeline has fully
    // drained after HALT is fetched (so run() knows to stop).
    bool step(uint64_t cycleNum);

    // Stage implementations. Read the CURRENT latch (cur) and write the
    // "next" output latch (next).
    void stageWB(Hart& hart);
    void stageMEM(Hart& hart, EX_MEM_Latch& cur, MEM_WB_Latch& next);
    void stageEX(Hart& hart, ID_EX_Latch& cur, EX_MEM_Latch& next);
    void stageID(Hart& hart, IF_ID_Latch& cur, ID_EX_Latch& next);
    void stageIF(Hart& hart, IF_ID_Latch& next);

    // Given a register number and the raw value read for it back in ID,
    // return the value EX should actually use.
    int32_t forwardOperand(Hart& hart, int regNum, int32_t rawVal,
                            const EX_MEM_Latch& exmem, const MEM_WB_Latch& memwb,
                            bool& forwardedFromEx, bool& forwardedFromMem);

    void logStage(uint64_t cycle, int hartId, uint32_t pc, const std::string& text, const char* stageName);
};
