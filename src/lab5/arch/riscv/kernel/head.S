.extern start_kernel
.extern _traps
.extern setup_vm
.extern mm_init
.extern setup_vm_final
.extern task_init

    .section .text.init
    .globl _start
_start:
    # YOUR CODE HERE

    # ------------------

    # let sp point to the top of the stack
    la sp, boot_stack_top
    li s0, 0xffffffdf80000000   # PA2VA_OFFSET
    sub sp, sp, s0              # sub offset to get physical address
                                # rather than virtual address got from GOT

    # do work after stack is ready

    # virtual memory setup
    jal setup_vm
    jal relocate

    # call mm_init to init memory system
    jal mm_init

    # call setup_vm_final to setup page table
    jal setup_vm_final

    # call task_init to init tasks
    jal task_init

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
    # csrr t0, sstatus
    # ori t0, t0, 0x2
    # csrw sstatus, t0

    # jump to kernel entry
    jal zero, start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    # #define PHY_START 0x0000000080000000
    # #define VM_START (0xffffffe000000000)
    # #define PA2VA_OFFSET (VM_START - PHY_START)

    ###################### 
    #   YOUR CODE HERE   #
    ######################
    # la t0, PA2VA_OFFSET
    add ra, ra, s0
    add sp, sp, s0

    # # set stvec to symbol _after_satp
    # la t0, _after_satp
    # csrw stvec, t0

    # set satp with early_pgtbl

    ###################### 
    #   YOUR CODE HERE   #
    ######################
    li t0, 0x8000000000000000   # mode: sv39, asid: 0
    la t1, early_pgtbl
    sub t1, t1, s0
    srli t1, t1, 12
    or t0, t0, t1
    csrw satp, t0

    # flush tlb
# _after_satp:
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: