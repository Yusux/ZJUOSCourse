#include "syscall.h"
#include "printk.h"

// implement sys_write here
size_t sys_write(unsigned int fd, const char* buf, size_t count) {
    if (fd == 1) {
        for (size_t i = 0; i < count; i++) {
            printk("%c", buf[i]);
        }
        return count;
    } else {
        return -1;
    }
}

// implement sys_getpid here
uint64_t sys_getpid() {
    return current->pid;
}

// implement syscall here
void syscall(struct pt_regs *regs) {
    switch (regs->x[17]) {
        case SYS_WRITE:
            regs->x[10] = sys_write(regs->x[10], (const char*)regs->x[11], regs->x[12]);
            break;
        case SYS_GETPID:
            regs->x[10] = sys_getpid();
            break;
        default:
            break;
    }
    regs->sepc += 4;
}