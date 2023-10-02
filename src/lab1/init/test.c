#include "printk.h"
#include "defs.h"

// Please do not modify

void test() {
    long long a = csr_read(sstatus);
    printk("sstatus = %llx\n", a);
    csr_write(sscratch, 0x0123456789abcdef);
    a = csr_read(sscratch);
    printk("sscratch = %llx\n", a);
    while (1) {
        printk("kernel is running!\n");
        // wait for a while
        for (int i = 0; i < 385000000; i++);
    }
}
