# dot_product.s -- dot product of two 6-element integer vectors.
# RV32I has no MUL instruction (that's the M extension), so this uses a
# repeated-addition multiply subroutine. That gives a nested-loop, branch-
# heavy workload useful for stressing the BHT with the *same* branch PCs
# reused across many outer-loop iterations (predictor aliasing/reuse).

    li a0, 0            # base of vector A
    li a1, 100           # base of vector B
    li a2, 6              # n

    li t0,1
    sw t0,0(a0)
    li t0,2
    sw t0,4(a0)
    li t0,3
    sw t0,8(a0)
    li t0,4
    sw t0,12(a0)
    li t0,5
    sw t0,16(a0)
    li t0,6
    sw t0,20(a0)

    li t0,10
    sw t0,0(a1)
    li t0,20
    sw t0,4(a1)
    li t0,30
    sw t0,8(a1)
    li t0,40
    sw t0,12(a1)
    li t0,50
    sw t0,16(a1)
    li t0,60
    sw t0,20(a1)

    li t1, 0             # i
    li t2, 0             # running dot-product sum
loop:
    bge  t1, a2, done
    slli t3, t1, 2
    add  t4, a0, t3
    add  t5, a1, t3
    lw   t6, 0(t4)         # A[i]
    lw   s0, 0(t5)         # B[i]

    li   s1, 0              # product accumulator
    mv   s2, s0              # multiply counter = B[i]
mul_loop:
    beqz s2, mul_done
    add  s1, s1, t6
    addi s2, s2, -1
    j    mul_loop
mul_done:
    add  t2, t2, s1
    addi t1, t1, 1
    j    loop
done:
    sw   t2, 200(a0)         # store final dot product
    nop
