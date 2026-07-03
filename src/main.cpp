#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "assembler.h"
#include "simulator.h"

namespace {

void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <program.s> [--diagram] [--diagram-cycles N] [--trace out.csv] [--maxcycles N]"
                 " [--dump-regs] [--dump-mem ADDR,COUNT]\n"
              << "  " << prog << " --compare <baseline.s> <optimized.s>\n";
}

SimulationStats runOne(const std::string& path, bool diagram, uint64_t diagramCycles,
                        const std::string& traceCsv, uint64_t maxCycles, bool printHeader = true,
                        bool dumpRegs = false, const std::string& dumpMemSpec = "") {
    auto program = Assembler::assembleFile(path);
    Simulator sim(std::move(program));
    sim.run(maxCycles);

    if (printHeader) std::cout << "\n### " << path << " ###\n";
    if (diagram) {
        sim.printDiagram(std::cout, diagramCycles);
        std::cout << "\n";
    }
    sim.printStats(std::cout);
    if (!traceCsv.empty()) {
        sim.writeTraceCsv(traceCsv);
        std::cout << "\n(trace written to " << traceCsv << ")\n";
    }
    if (dumpRegs) {
        std::cout << "\n-- Registers --\n";
        for (int i = 0; i < 32; i++) {
            std::cout << "x" << i << "=" << sim.registers().read(i) << (i % 4 == 3 ? "\n" : "\t");
        }
        std::cout << "\n";
    }
    if (!dumpMemSpec.empty()) {
        size_t comma = dumpMemSpec.find(',');
        uint32_t addr = (uint32_t)std::stoul(dumpMemSpec.substr(0, comma), nullptr, 0);
        int count = comma == std::string::npos ? 1 : std::stoi(dumpMemSpec.substr(comma + 1));
        std::cout << "\n-- Memory dump from 0x" << std::hex << addr << std::dec << " --\n";
        for (int i = 0; i < count; i++) {
            std::cout << "[" << (addr + i * 4) << "] = " << sim.memory().readWord(addr + i * 4) << "\n";
        }
    }
    return sim.stats();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    std::vector<std::string> args(argv + 1, argv + argc);

    if (args[0] == "--compare") {
        if (args.size() < 3) { printUsage(argv[0]); return 1; }
        try {
            auto baseline = runOne(args[1], false, 60, "", 2'000'000);
            auto optimized = runOne(args[2], false, 60, "", 2'000'000);
            std::cout << "\n===== Comparison =====\n";
            std::cout << "Baseline CPI:  " << baseline.cpi() << "  (" << baseline.cycles << " cycles / "
                       << baseline.instructionsRetired << " instrs)\n";
            std::cout << "Optimized CPI: " << optimized.cpi() << "  (" << optimized.cycles << " cycles / "
                       << optimized.instructionsRetired << " instrs)\n";
            double cpiImprovement = (baseline.cpi() - optimized.cpi()) / baseline.cpi() * 100.0;
            double cycleImprovement = (1.0 - (double)optimized.cycles / (double)baseline.cycles) * 100.0;
            std::cout << "CPI improvement:    " << cpiImprovement << "%\n";
            std::cout << "Cycle-count change: " << cycleImprovement << "%\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    std::string path = args[0];
    bool diagram = false;
    uint64_t diagramCycles = 80;
    std::string traceCsv;
    uint64_t maxCycles = 2'000'000;
    bool dumpRegs = false;
    std::string dumpMemSpec;

    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "--diagram") diagram = true;
        else if (args[i] == "--diagram-cycles" && i + 1 < args.size()) diagramCycles = std::stoull(args[++i]);
        else if (args[i] == "--trace" && i + 1 < args.size()) traceCsv = args[++i];
        else if (args[i] == "--maxcycles" && i + 1 < args.size()) maxCycles = std::stoull(args[++i]);
        else if (args[i] == "--dump-regs") dumpRegs = true;
        else if (args[i] == "--dump-mem" && i + 1 < args.size()) dumpMemSpec = args[++i];
        else { std::cerr << "Unknown option: " << args[i] << "\n"; printUsage(argv[0]); return 1; }
    }

    try {
        runOne(path, diagram, diagramCycles, traceCsv, maxCycles, false, dumpRegs, dumpMemSpec);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
