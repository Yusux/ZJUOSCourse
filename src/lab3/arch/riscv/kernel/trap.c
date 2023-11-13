// trap.c
#include "trap.h"
#include "printk.h"
extern void do_timer(void);

void trap_handler(unsigned long scause, unsigned long sepc) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略

    // According to https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf, Table 4.2
    // printk("[S] Supervisor Mode Cause: %lx\n", scause);
    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 5) {
        // Timer interrupt occurs, set next timer interrupt
        clock_set_next_event();
        // Call do_timer() to update task status
        do_timer();
        // Print time interrupt info
        printk("[S] Supervisor Mode Timer Interrupt\n");
    }

}