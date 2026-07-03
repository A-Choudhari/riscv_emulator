# bubble_sort.s -- bubble sort of 8 integers.
# Exercises: nested loops, load-use hazards (lw immediately followed by a
# compare that reads it), forwarding across ALU/address computation chains.

    li a0, 0            # array base address

    li t0, 5
    sw t0, 0(a0)
    li t0, 3
    sw t0, 4(a0)
    li t0, 8
    sw t0, 8(a0)
    li t0, 1
    sw t0, 12(a0)
    li t0, 9
    sw t0, 16(a0)
    li t0, 2
    sw t0, 20(a0)
    li t0, 7
    sw t0, 24(a0)
    li t0, 4
    sw t0, 28(a0)

    li a1, 8             # n
    li t1, 0              # i = 0
outer:
    addi t2, a1, -1
    bge  t1, t2, outer_done
    li   t3, 0              # j = 0
    sub  t4, a1, t1
    addi t4, t4, -1          # inner bound = n-1-i
inner:
    bge  t3, t4, inner_done
    slli t5, t3, 2
    add  t6, a0, t5
    lw   s0, 0(t6)
    lw   s1, 4(t6)
    blt  s1, s0, do_swap
    j    no_swap
do_swap:
    sw   s1, 0(t6)
    sw   s0, 4(t6)
no_swap:
    addi t3, t3, 1
    j    inner
inner_done:
    addi t1, t1, 1
    j    outer
outer_done:
    nop
