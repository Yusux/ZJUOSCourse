#ifndef _SYSCALL_H
#define _SYSCALL_H

#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_GETPID 172
#define SYS_CLONE 220

#include "stdint.h"
#include "stddef.h"
#include "trap.h"
#include "proc.h"

int64_t sys_read(unsigned int fd, char* buf, uint64_t count);
int64_t sys_write(unsigned int fd, const char* buf, size_t count);
uint64_t sys_getpid();

void syscall(struct pt_regs *regs);

#endif
