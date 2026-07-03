# fibonacci.s -- iterative Fibonacci, stores first 12 numbers to memory.
# Exercises: a tight backward-branch loop (mostly-taken pattern -- good
# case for the 2-bit predictor), simple register-to-register forwarding.

    li   a0, 0          # base address for results
    li   a1, 12         # how many Fibonacci numbers to compute
    li   t0, 0          # fib(0)
    li   t1, 1          # fib(1)
    sw   t0, 0(a0)
    sw   t1, 4(a0)
    li   t3, 2          # index counter, starts at 2

loop:
    bge  t3, a1, done
    add  t2, t0, t1     # fib(i) = fib(i-1) + fib(i-2)
    slli t4, t3, 2
    add  t5, a0, t4
    sw   t2, 0(t5)
    mv   t0, t1
    mv   t1, t2
    addi t3, t3, 1
    j    loop

done:
    nop
