#ifndef _TRAP_H
#define _TRAP_H

unsigned long get_cycles();

void clock_set_next_event();

void trap_handler(unsigned long scause, unsigned long sepc);
 
#endif
