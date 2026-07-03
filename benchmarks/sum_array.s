# sum_array.s -- baseline (unoptimized) array sum.
# Every iteration does: load element, then IMMEDIATELY add it to the
# running sum -- a guaranteed load-use hazard/stall every single
# iteration. This is the intentional "before" case for the loop-unrolling
# / instruction-scheduling optimization demonstrated in
# sum_array_unrolled.s -- compare the two with:
#   ./rvsim --compare benchmarks/sum_array.s benchmarks/sum_array_unrolled.s

    li a0, 0
    li a1, 16          # n elements
    li t0, 0
init:
    bge  t0, a1, init_done
    slli t1, t0, 2
    add  t2, a0, t1
    sw   t0, 0(t2)       # array[i] = i
    addi t0, t0, 1
    j    init
init_done:

    li t3, 0            # sum
    li t0, 0             # i
loop:
    bge  t0, a1, done
    slli t1, t0, 2
    add  t2, a0, t1
    lw   t4, 0(t2)
    add  t3, t3, t4       # <-- load-use hazard: t4 used the instruction right after loading
    addi t0, t0, 1
    j    loop
done:
    sw   t3, 100(a0)
    nop
