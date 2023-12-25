#include "printk.h"
#include "sbi.h"

extern void test();
extern void schedule();

int start_kernel() {
    printk("[S] 2023");
    printk(" Hello RISC-V\n");

    // schedule at the beginning
    schedule();

	return 0;
}
