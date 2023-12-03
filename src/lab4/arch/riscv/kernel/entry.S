.extern trap_handler
.extern dummy

# 与 ARM 架构不同的是，RISC-V 中只有一个栈指针寄存器( sp )，因此需要我们来完成用户栈与内核栈的切换。
# 由于我们的用户态进程运行在 U-Mode 下， 使用的运行栈也是 U-Mode Stack， 因此当触发异常时， 我们首先要对栈进行切换 （ U-Mode Stack -> S-Mode Stack ）。同理 让我们完成了异常处理， 从 S-Mode 返回至 U-Mode， 也需要进行栈切换 （ S-Mode Stack -> U-Mode Stack ）。
# 修改 __dummy。在 4.2 中 我们初始化时， thread_struct.sp 保存了 S-Mode sp， thread_struct.sscratch 保存了 U-Mode sp， 因此在 S-Mode -> U->Mode 的时候，我们只需要交换对应的寄存器的值即可。
# 修改 _trap 。同理 在 _trap 的首尾我们都需要做类似的操作。注意如果是 内核线程( 没有 U-Mode Stack ) 触发了异常，则不需要进行切换。（内核线程的 sp 永远指向的 S-Mode Stack， sscratch 为 0）
# uapp 使用 ecall 会产生 ECALL_FROM_U_MODE exception。因此我们需要在 trap_handler 里面进行捕获。修改 trap_handler 如下：

    .section .text.entry
    .align 2
    .globl _traps 
_traps:
    # YOUR CODE HERE
    # -----------
    # 0. switch to kernel stack
    csrr t0, sscratch
    beq t0, zero, _switched_to_kernel_stack
    csrw sscratch, sp
    mv sp, t0

_switched_to_kernel_stack:
    # 1. save 32 registers and sepc to stack
    sd x0,  -264(sp)
    sd x1,  -256(sp)
    sd x2,  -248(sp)
    sd x3,  -240(sp)
    sd x4,  -232(sp)
    sd x5,  -224(sp)
    sd x6,  -216(sp)
    sd x7,  -208(sp)
    sd x8,  -200(sp)
    sd x9,  -192(sp)
    sd x10, -184(sp)
    sd x11, -176(sp)
    sd x12, -168(sp)
    sd x13, -160(sp)
    sd x14, -152(sp)
    sd x15, -144(sp)
    sd x16, -136(sp)
    sd x17, -128(sp)
    sd x18, -120(sp)
    sd x19, -112(sp)
    sd x20, -104(sp)
    sd x21,  -96(sp)
    sd x22,  -88(sp)
    sd x23,  -80(sp)
    sd x24,  -72(sp)
    sd x25,  -64(sp)
    sd x26,  -56(sp)
    sd x27,  -48(sp)
    sd x28,  -40(sp)
    sd x29,  -32(sp)
    sd x30,  -24(sp)
    sd x31,  -16(sp)
    csrr t0, sepc
    sd t0,    -8(sp)
    addi sp, sp, -264

    # -----------

    # 2. call trap_handler
    csrr a0, scause # scause -> a0
    csrr a1, sepc   # sepc -> a1
    mv a2, sp     # view part of stack as pt_regs
    jal ra, trap_handler

    # -----------

    # 3. restore sepc and 32 registers from stack
    ld t0,  256(sp)
    csrw sepc, t0
    ld x31, 248(sp)
    ld x30, 240(sp)
    ld x29, 232(sp)
    ld x28, 224(sp)
    ld x27, 216(sp)
    ld x26, 208(sp)
    ld x25, 200(sp)
    ld x24, 192(sp)
    ld x23, 184(sp)
    ld x22, 176(sp)
    ld x21, 168(sp)
    ld x20, 160(sp)
    ld x19, 152(sp)
    ld x18, 144(sp)
    ld x17, 136(sp)
    ld x16, 128(sp)
    ld x15, 120(sp)
    ld x14, 112(sp)
    ld x13, 104(sp)
    ld x12,  96(sp)
    ld x11,  88(sp)
    ld x10,  80(sp)
    ld x9,   72(sp)
    ld x8,   64(sp)
    ld x7,   56(sp)
    ld x6,   48(sp)
    ld x5,   40(sp)
    ld x4,   32(sp)
    ld x3,   24(sp)
    ld x1,    8(sp)
    ld x2,   16(sp) # load sp at last

    # -----------

    # 4. return from trap
    csrr t0, sscratch
    beq t0, zero, _switched_to_stack_before_trap
    csrw sscratch, sp
    mv sp, t0

_switched_to_stack_before_trap:
    sret

    # -----------

    .global __dummy
__dummy:
    # YOUR CODE HERE

    # 1. swap sp and sscratch
    csrr t0, sscratch
    csrw sscratch, sp
    mv sp, t0

    # 2. return to the start of the function
    # csrw sepc, zero # set sepc to zero
    sret

    
    .globl __switch_to
__switch_to: 
    # save state to prev process
    # YOUR CODE HERE
    sd ra,   48(a0)
    sd sp,   56(a0)
    sd s0,   64(a0)
    sd s1,   72(a0)
    sd s2,   80(a0)
    sd s3,   88(a0)
    sd s4,   96(a0)
    sd s5,  104(a0)
    sd s6,  112(a0)
    sd s7,  120(a0)
    sd s8,  128(a0)
    sd s9,  136(a0)
    sd s10, 144(a0)
    sd s11, 152(a0)
    # save sepc sstatus sscratch
    csrr t0, sepc
    sd t0, 160(a0)
    csrr t0, sstatus
    sd t0, 168(a0)
    csrr t0, sscratch
    sd t0, 176(a0)


    # restore state from next process
    # YOUR CODE HERE
    ld ra,   48(a1)
    ld sp,   56(a1)
    ld s0,   64(a1)
    ld s1,   72(a1)
    ld s2,   80(a1)
    ld s3,   88(a1)
    ld s4,   96(a1)
    ld s5,  104(a1)
    ld s6,  112(a1)
    ld s7,  120(a1)
    ld s8,  128(a1)
    ld s9,  136(a1)
    ld s10, 144(a1)
    ld s11, 152(a1)
    # restore sepc sstatus sscratch
    ld t0,  160(a1)
    csrw sepc, t0
    ld t0,  168(a1)
    csrw sstatus, t0
    ld t0,  176(a1)
    csrw sscratch, t0

    # switch page table
    ld t0,  184(a1)             # the page table physical address of next process
    li t1, 0x8000000000000000   # mode: sv39, asid: 0
    srli t0, t0, 12             # shift right 12 bits
    or t0, t0, t1               # set mode and asid
    csrw satp, t0

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret