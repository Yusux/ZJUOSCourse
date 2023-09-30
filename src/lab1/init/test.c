#include "printk.h"
#include "defs.h"

// Please do not modify

void test() {
    while (1) {
        printk("kernel is running!\n");
        // wait for a while
        for (int i = 0; i < 385000000; i++);
    }
}
