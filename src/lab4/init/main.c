#include "printk.h"
#include "sbi.h"

extern void test();
extern void schedule();

int start_kernel() {
    printk("2022");
    printk(" Hello RISC-V\n");

    // schedule at the beginning
    schedule();

    test(); // DO NOT DELETE !!!

	return 0;
}
