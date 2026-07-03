# stride_access.s -- memory-intensive strided access pattern.
# Initializes a 64-element array, then sums every 4th element. High
# load-to-ALU-op ratio, very predictable single-loop branch (great case
# for the predictor), good baseline for MEM/EX forwarding utilization.

    li a0, 0
    li a1, 64
    li t0, 0
init_loop:
    bge  t0, a1, init_done
    slli t1, t0, 2
    add  t2, a0, t1
    sw   t0, 0(t2)         # array[i] = i
    addi t0, t0, 1
    j    init_loop
init_done:

    li t3, 0                # sum
    li t4, 0                # byte offset, stride = 4 elements = 16 bytes
    li t5, 256                # 64 elements * 4 bytes
stride_loop:
    bge  t4, t5, stride_done
    add  t6, a0, t4
    lw   s0, 0(t6)
    add  t3, t3, s0
    addi t4, t4, 16
    j    stride_loop
stride_done:
    sw   t3, 300(a0)
    nop
