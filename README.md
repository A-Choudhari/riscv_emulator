# RV32I 5-Stage Pipeline Simulator (starter code)

Scaffolding for a cycle-accurate simulator of a classic 5-stage RISC-V
pipeline (IF -> ID -> EX -> MEM -> WB), in C++17 with no dependencies
beyond the STL, plus a Python/matplotlib visualizer for the pipeline
diagram.

**What's done for you (boring plumbing):** the assembler, register file,
Harvard-split instruction/data memory, the CLI driver, and the
stats/diagram/trace printing. It all compiles and the assembler will
correctly turn the bundled benchmarks into instruction streams.

**What you need to implement (the actual project):** everything in
`src/simulator.cpp` and `include/branch_predictor.h` -- the pipeline
stage logic, hazard detection, forwarding, and branch prediction. Right
now `Simulator::step()` just throws `"not implemented yet"`. Every
function you need to fill in has a `TODO` describing its contract; look
there first.

## Quick start

```bash
make                # builds ./rvsim (compiles clean right now)
./rvsim benchmarks/fibonacci.s
# -> Error: TODO: Simulator::step() not implemented yet
```

Once you've implemented the pipeline:

```bash
./rvsim benchmarks/fibonacci.s --diagram --diagram-cycles 60
./rvsim benchmarks/bubble_sort.s --trace trace.csv
python3 viz/pipeline_diagram.py trace.csv --out diagram.png
./rvsim benchmarks/sum_array.s --dump-mem 100,1     # should print 120 once correct
./rvsim --compare benchmarks/sum_array.s benchmarks/sum_array_unrolled.s
```

CLI flags:
```
./rvsim <program.s> [--diagram] [--diagram-cycles N] [--trace out.csv]
                     [--maxcycles N] [--dump-regs] [--dump-mem ADDR,COUNT]
./rvsim --compare <baseline.s> <optimized.s>
```

## Project layout

```
include/
  types.h             DONE. Op enum, Instruction struct, OpClass helpers
                       (classify/writesRegister/usesRs1/usesRs2/isBranch/isMemOp)
  register_file.h      DONE. 32 x 32-bit regs, x0 hardwired to 0
  memory.h              DONE. InstructionMemory + DataMemory (Harvard split)
  assembler.h            DONE. two-pass assembler (labels -> Instruction list)
  branch_predictor.h  TODO. 2-bit saturating counter BHT -- class skeleton only
  simulator.h         Latch structs + Simulator class declaration. Stage
                       function SIGNATURES are here; you fill in bodies in
                       simulator.cpp. Feel free to add fields/private state
                       as your design needs them.
src/
  types.cpp, assembler.cpp    DONE.
  simulator.cpp        TODO -- this is the project. stageWB/stageMEM/stageEX/
                        stageID/stageIF/forwardOperand/step are all stubs.
                        printStats/printDiagram/writeTraceCsv/logStage are
                        implemented for you (pure formatting, no pipeline logic).
  main.cpp              DONE. CLI driver.
benchmarks/           7 RISC-V assembly programs, described below, each with
                      an expected final result in a comment -- use
                      --dump-mem to self-check your implementation once it runs.
viz/
  pipeline_diagram.py   DONE. renders a trace CSV as the colored grid, once
                         your simulator produces one via --trace.
Makefile
```

## What you need to design and build

**Data hazards.**
- EX/MEM -> EX and MEM/WB -> EX forwarding (`Simulator::forwardOperand`).
- Load-use hazard detection: when the instruction entering EX is a load
  whose destination is a source register of the instruction currently in
  ID, forwarding can't help (load data isn't ready until MEM completes)
  -- you need to stall for a cycle instead (freeze PC + IF/ID, bubble
  into ID/EX).

**Control hazards.**
- Implement the 2-bit saturating counter + BHT in `branch_predictor.h`.
- Decide where prediction happens (IF or ID) and where resolution happens
  (typically EX, once operands are available via forwarding) -- this
  determines your bubble penalties for correctly- vs. incorrectly-
  predicted branches. Think through: what happens when an EX-stage
  misprediction (an older instruction) and an ID-stage redirect (a
  younger instruction) both want to redirect PC in the same cycle?
- `JAL`'s target is fully known from the instruction alone (no register
  read needed) -- can it be resolved earlier than a conditional branch?
- `JALR`'s target depends on a register value -- when's the earliest it
  can be resolved?

**Structural hazards.** Already eliminated by construction: `imem_` and
`dmem_` are separate objects, so IF and MEM can never contend for a
memory port. Nothing to do here, but it's worth understanding *why*
that's true when you write it up.

**Stats to populate as you go:** `SimulationStats` in `simulator.h` --
CPI, retired instructions, load-use stall cycles, control-hazard bubble
cycles, branch resolved/mispredict counts, forwarding-path utilization.
`printStats()` already knows how to format all of these; it's on you to
increment them in the right places.

## Design constraints already baked into the given code

- `Assembler` parses `.s` files straight into `Instruction` structs
  (opcode enum + integer fields) -- there's no real 32-bit RISC-V binary
  encoding here, which keeps you focused on microarchitecture rather than
  bit-twiddling an encoder/decoder. (Good stretch goal if you want it later.)
- No M-extension (`mul`/`div`); `dot_product.s` does multiply via a
  repeated-addition subroutine instead.
- `lui`/`auipc` immediates are the raw upper-20-bit value, matching real
  RISC-V asm convention (`lui a0, 0x10` -> `a0 = 0x10000`).
- Branch/JAL immediates are precomputed by the assembler as
  `target_addr - this_instr_addr`, i.e. already the PC-relative offset --
  so resolving one is just `pc + imm`.

## Benchmarks

Each file has an expected final result noted in a comment, so you can
self-check correctness with `--dump-mem` once your pipeline runs
end-to-end (independent of whether your hazard/forwarding logic is
correct -- get functional correctness first, then verify the *stats*
make sense).

| File | Exercises | Expected result |
|---|---|---|
| `fibonacci.s` | tight backward-branch loop, mostly-taken | mem[0..44] = 0,1,1,2,3,5,8,13,21,34,55,89 |
| `bubble_sort.s` | nested loops, load-use hazards, forwarding | mem[0..28] = 1,2,3,4,5,7,8,9 |
| `dot_product.s` | nested loop, repeated-addition multiply, BHT reuse across outer iterations | mem[200] = 910 |
| `stride_access.s` | load-heavy strided memory access | mem[300] = 480 |
| `sum_array.s` | baseline: guaranteed load-use stall every iteration | mem[100] = 120 |
| `sum_array_unrolled.s` | 4x-unrolled + scheduled version of the above | mem[100] = 120, but fewer cycles/stalls |
| `branch_heavy.s` | alternating (parity) branch -- near-worst-case for a 2-bit predictor | (no memory check; look at branch accuracy stat) |

`make test` runs all 7 (will currently fail with the TODO error until
you implement `step()`).

## The optimization writeup you'll build

Once your pipeline is correct, `--compare benchmarks/sum_array.s
benchmarks/sum_array_unrolled.s` should show the unrolled version with
zero load-use stalls (vs. one per iteration in the baseline) and fewer
total cycles (fewer loop-control instructions and their branch bubbles).
That's your "loop unrolling improves CPI" resume claim -- back it with
the actual numbers your simulator produces.

## Stretch goals

Once the base pipeline works:
1. Real RV32I binary encoding (encoder + decoder), loading actual
   `.bin`/objdump-derived instruction streams instead of assembler-internal structs.
2. A Branch Target Buffer (BTB) predicting *and* fetching the target in
   IF, removing the 1-cycle bubble on correctly-predicted taken branches;
   real `JALR` prediction (e.g. a return-address stack).
3. Superscalar/dual-issue -- fetch/decode/issue 2 instructions per cycle.
4. A simple cache model with hit/miss stats (`stride_access.s` is built
   to show interesting behavior once you add one).
5. Use the stats across all 7 benchmarks to write the "why
   microarchitecture decisions exist" analysis the project brief calls
   for -- e.g. CPI vs. branch density, or forwarding utilization vs. load
   density, across benchmarks.
