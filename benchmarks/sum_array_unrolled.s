# sum_array_unrolled.s -- OPTIMIZED version of sum_array.s.
# Optimization applied: 4x loop unrolling + instruction scheduling.
# Four independent loads are issued back-to-back before any of them are
# consumed by an add, giving forwarding paths (and, in a real superscalar
# machine, other independent work) enough distance to hide the load-to-use
# latency entirely. This also cuts loop-branch overhead 4x (one taken
# branch per 4 elements instead of per element).
#
# Compare against the baseline with:
#   ./rvsim --compare benchmarks/sum_array.s benchmarks/sum_array_unrolled.s

    li a0, 0
    li a1, 16          # n elements
    li t0, 0
init:
    bge  t0, a1, init_done
    slli t1, t0, 2
    add  t2, a0, t1
    sw   t0, 0(t2)
    addi t0, t0, 1
    j    init
init_done:

    li t3, 0             # sum
    li t0, 0              # i, steps by 4
    li a2, 16               # n (must be a multiple of the unroll factor here)
unroll_loop:
    bge  t0, a2, tail
    slli t1, t0, 2
    add  t2, a0, t1
    lw   t4, 0(t2)
    lw   t5, 4(t2)
    lw   t6, 8(t2)
    lw   s0, 12(t2)
    add  t3, t3, t4        # by now t4's load is 4 instructions in the past -- no stall
    add  t3, t3, t5
    add  t3, t3, t6
    add  t3, t3, s0
    addi t0, t0, 4
    j    unroll_loop
tail:
    sw   t3, 100(a0)
    nop
