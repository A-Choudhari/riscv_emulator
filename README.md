# RV32I 5-Stage Pipeline Simulator

A **cycle-accurate simulator** of a classic 5-stage RISC-V pipeline (IF → ID → EX → MEM → WB), written in C++17 with no dependencies beyond the STL. Supports multi-hart (multi-core) parallel execution, atomic memory operations, full data/control hazard handling, and per-hart statistics reporting. Includes a Python/matplotlib pipeline diagram visualizer.

---

## Features

- **Full RV32I ISA** — all ALU, load/store, branch, jump, and upper-immediate instructions
- **Hazard Detection & Forwarding** — EX/MEM → EX and MEM/WB → EX data forwarding; load-use stall insertion with correct bubble propagation
- **2-Bit Saturating Branch Predictor** — per-hart Branch History Table (BHT) with ID-stage prediction and EX-stage resolution; achieves up to 97.6% accuracy on loop-heavy workloads
- **Multi-Hart Parallel Execution** — configurable number of independent hardware threads sharing a unified data memory; each hart has its own register file, branch predictor, pipeline latches, and statistics
- **Atomic Swap (`amoswap.w`)** — RV32A atomic read-modify-write instruction enabling spinlock mutexes and inter-hart synchronization in assembly
- **Per-Hart Statistics** — CPI, retired instructions, load-use stall cycles, control-hazard bubbles, forwarding utilization, and branch accuracy reported per-hart and in aggregate
- **Cycle-Accurate Pipeline Trace** — CSV trace of every instruction's stage assignment every cycle, renderable as a colored pipeline diagram

---

## Building

Requires `g++` with C++17 support.

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude \
    src/types.cpp src/assembler.cpp src/simulator.cpp src/main.cpp \
    -o rvsim
```

Or with the provided Makefile (Linux/macOS):

```bash
make
```

---

## Running

```bash
# Basic simulation
./rvsim benchmarks/fibonacci.s

# Multi-hart parallel execution (2 cores)
./rvsim benchmarks/parallel_sum.s --harts 2 --dump-mem 104,1

# Generate an ASCII pipeline diagram (first 60 cycles)
./rvsim benchmarks/bubble_sort.s --diagram --diagram-cycles 60

# Export trace CSV and render as a visual pipeline diagram
./rvsim benchmarks/sum_array.s --trace trace.csv
python3 viz/pipeline_diagram.py trace.csv --out diagram.png

# Dump memory contents after execution
./rvsim benchmarks/dot_product.s --dump-mem 200,1

# Dump register file for all harts
./rvsim benchmarks/parallel_sum.s --harts 2 --dump-regs

# Compare two programs side-by-side (CPI, cycle count, improvement %)
./rvsim --compare benchmarks/sum_array.s benchmarks/sum_array_unrolled.s

# Cap the simulation at N cycles
./rvsim benchmarks/branch_heavy.s --maxcycles 500
```

### CLI Reference

```
./rvsim <program.s> [--harts N] [--diagram] [--diagram-cycles N]
                    [--trace out.csv] [--maxcycles N]
                    [--dump-regs] [--dump-mem ADDR,COUNT]

./rvsim --compare <baseline.s> <optimized.s>
```

---

## Benchmarks

Eight assembly programs covering a range of microarchitectural behaviors. All single-hart programs produce verified memory outputs checkable with `--dump-mem`.

| File | Exercises | Expected Output |
|---|---|---|
| `fibonacci.s` | Tight backward-branch loop, mostly-taken branches | `mem[0..44]` = 0,1,1,2,3,5,8,13,21,34,55,89 |
| `bubble_sort.s` | Nested loops, load-use hazards, forwarding | `mem[0..28]` = 1,2,3,4,5,7,8,9 |
| `dot_product.s` | Repeated-addition multiply, BHT reuse across loop iterations | `mem[200]` = 910 |
| `stride_access.s` | Load-heavy strided memory access pattern | `mem[300]` = 480 |
| `sum_array.s` | Baseline: one load-use stall guaranteed per iteration | `mem[100]` = 120 |
| `sum_array_unrolled.s` | 4×-unrolled + scheduled: zero load-use stalls | `mem[100]` = 120, fewer cycles |
| `branch_heavy.s` | Alternating parity branch — near-worst-case for a 2-bit predictor | Branch accuracy stat |
| `parallel_sum.s` | Two harts synchronizing via `amoswap.w` spinlock to accumulate a shared sum | `mem[104]` = 30 (with `--harts 2`) |

---

## Project Layout

```
include/
  types.h             Op enum, Instruction struct, OpClass helpers
  register_file.h     32 × 32-bit registers, x0 hardwired to 0
  memory.h            InstructionMemory + DataMemory (Harvard split)
  assembler.h         Two-pass assembler (labels → Instruction list)
  branch_predictor.h  2-bit saturating counter BHT, per-hart
  simulator.h         Hart struct, pipeline latch structs, Simulator class
src/
  types.cpp           OpClass classification and register trait helpers
  assembler.cpp       Full RV32I + amoswap.w instruction parser
  simulator.cpp       Pipeline stage implementations (IF/ID/EX/MEM/WB),
                      hazard detection, forwarding, multi-hart step loop,
                      stats aggregation, diagram/trace output
  main.cpp            CLI driver
benchmarks/           8 RISC-V assembly programs with known expected outputs
viz/
  pipeline_diagram.py Renders a trace CSV as a colored pipeline grid
Makefile
```

---

## Implementation Notes

- **Harvard Architecture** — instruction memory (`imem_`) and data memory (`dmem_`) are separate objects, eliminating structural hazards between the IF and MEM stages by construction
- **Two-Phase Latch Update** — all five stages compute into fresh "next" latch copies during each cycle; current latches are only overwritten at the end of `step()`, making all stages appear to execute simultaneously
- **Multi-Hart Memory Model** — harts share `dmem_` and are stepped sequentially within each simulated cycle; `amoswap.w` is therefore naturally atomic within the simulation without host-level locking
- **Assembler** — parses `.s` files directly into decoded `Instruction` structs rather than 32-bit binary, keeping the focus on microarchitecture rather than bit-packing. Supports all RV32I mnemonics plus pseudoinstructions (`li`, `mv`, `j`, `ret`, `beqz`, `bnez`) and `amoswap.w[.aq][.rl]`
