#ifndef _SYSCALL_H
#define _SYSCALL_H

#define SYS_WRITE 64
#define SYS_GETPID 172

#include "stdint.h"
#include "stddef.h"
#include "trap.h"
#include "proc.h"

extern struct pt_regs *trap_regs;
extern struct task_struct *current;

size_t sys_write(unsigned int fd, const char* buf, size_t count);
uint64_t sys_getpid();

void syscall(struct pt_regs *regs);

#endif
