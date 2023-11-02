.extern start_kernel
.extern _traps
.extern mm_init
.extern task_init

    .section .text.init
    .globl _start
_start:
    # YOUR CODE HERE

    # ------------------

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0

    # ------------------

    # set sie[STIE] = 1
    # According to https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf, Figure 4.5
    csrr t0, sie
    ori t0, t0, 0x20
    csrw sie, t0

    # ------------------

    # set first time interrupt
    rdtime t0
    li t1, 10000000
    add a0, t0, t1
    add a6, zero, zero
    add a7, zero, zero
    ecall

    # ------------------

    # set sstatus[SIE] = 1
    # According to https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf, Figure 4.2
    csrr t0, sstatus
    ori t0, t0, 0x2
    csrw sstatus, t0

    # ------------------

    # ------------------
    # - your lab1 code -
    # ------------------

    # let sp point to the top of the stack
    la sp, boot_stack_top

    # ------------------
    # - your lab2 code -
    # ------------------
    # do work after stack is ready

    # call mm_init to init memory system
    jal mm_init

    # call task_init to init tasks
    jal task_init

    # jump to kernel entry
    jal zero, start_kernel

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: