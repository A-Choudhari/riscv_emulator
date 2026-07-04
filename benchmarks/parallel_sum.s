# parallel_sum.s - Sums an array in parallel using 2 harts
    # a0 contains the hart ID (0 or 1)
    li   t0, 0
    beq  a0, t0, hart0
hart1:
    # Hart 1 logic (sum odd indices)
    li   t1, 10             # local sum accumulator
    j    accumulate
hart0:
    # Hart 0 logic (sum even indices)
    li   t1, 20             # local sum accumulator
accumulate:
    # Both harts need to add t1 to the shared memory address 100
    li   a1, 200            # Address of the mutex/lock (0 = unlocked, 1 = locked)
    li   a2, 104            # Address of global sum
acquire_lock:
    li   t2, 1
    amoswap.w t3, t2, (a1)  # Swap t2 (1) with lock variable at a1
    bnez t3, acquire_lock   # If lock was already held (returned 1), retry
critical_section:
    lw   t4, 0(a2)          # Read current global sum
    add  t4, t4, t1         # Add local sum
    sw   t4, 0(a2)          # Write back
release_lock:
    sw   zero, 0(a1)        # Unlock by setting lock variable to 0
    nop