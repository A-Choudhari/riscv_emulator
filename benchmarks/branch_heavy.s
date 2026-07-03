# branch_heavy.s -- alternating (data-dependent) branch pattern.
# The inner branch on parity flips taken/not-taken every iteration, which
# is close to a worst case for a simple 2-bit saturating counter (it tends
# to mispredict on every flip). Contrast its accuracy against the
# consistently mostly-taken loop-back branches in the other benchmarks to
# illustrate predictor strengths/weaknesses in your writeup.

    li a0, 0
    li t0, 0            # i
    li t1, 20             # iterations
loop:
    bge  t0, t1, done
    andi t2, t0, 1
    beqz t2, is_even
    addi t3, t0, 100
    j    after
is_even:
    addi t3, t0, 200
after:
    slli t4, t0, 2
    add  t5, a0, t4
    sw   t3, 0(t5)
    addi t0, t0, 1
    j    loop
done:
    nop
