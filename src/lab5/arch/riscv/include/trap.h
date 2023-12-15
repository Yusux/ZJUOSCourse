#ifndef _TRAP_H
#define _TRAP_H

struct pt_regs {
    unsigned long x[32];
    unsigned long sepc;
    unsigned long sstatus;
    unsigned long stval;
    unsigned long sscratch;
    unsigned long scause;
};

unsigned long get_cycles();

void clock_set_next_event();

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs);

void do_page_fault(struct pt_regs *regs);

#endif
