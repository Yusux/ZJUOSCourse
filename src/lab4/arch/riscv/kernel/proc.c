//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"

//arch/riscv/kernel/proc.c

extern void __dummy();
extern void __switch_to(struct task_struct* prev, struct task_struct* next);
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组
extern unsigned long  swapper_pg_dir[];    // kernel pagetable 根目录， 在 setup_vm_final 进行映射。
extern char _sramdisk[];
extern char _eramdisk[];

void task_init() {
    printk("...task_init start...\n");
    test_init(NR_TASKS);
    printk("...test_init done...\n");
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    /* YOUR CODE HERE */
    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;

    current = idle;
    task[0] = idle;

    printk("Set idle done\n");

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    /* YOUR CODE HERE */
    for (int i = 1; i < NR_TASKS; i++) {
        printk("Set %d start\n", i);
        struct task_struct* task_ptr = (struct task_struct*)kalloc();
        task_ptr->state = TASK_RUNNING;
        task_ptr->counter = task_test_counter[i];
        task_ptr->priority = task_test_priority[i];
        task_ptr->pid = i;

        task_ptr->thread.ra = (uint64)__dummy;
        task_ptr->thread.sp = (uint64)task_ptr + PGSIZE;

        // 为 sepc, sstatus, sscratch 赋值
        task_ptr->thread.sepc = USER_START;
        task_ptr->thread.sstatus = (SSTATUS_SUM | SSTATUS_SPIE) & (~SSTATUS_SPP);
        task_ptr->thread.sscratch = USER_END;

        // 为每个进程创建属于它自己的页表
        printk("Set %d pgd\n", i);
        uint64 pgd = (uint64)alloc_page();
        // 为了避免 U-Mode 和 S-Mode 切换的时候切换页表，
        // 我们将内核页表 （ swapper_pg_dir ） 复制到每个进程的页表中。
        memcpy((void *)pgd, swapper_pg_dir, PGSIZE);

        // 将 uapp 所在的页面映射到每个进行的页表中
        printk("Set %d mapping\n", i);
        uint64 va = USER_START;
        uint64 sz = _eramdisk - _sramdisk;
        uint64 pg_num = PGROUNDUP(sz) / PGSIZE;
        uint64 perm = PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;
        // 申请专用内存，防止所有的进程共享数据，造成预期外的进程间相互影响
        uint64 uapp_mem = (uint64)alloc_pages(pg_num);
        printk("uapp_mem = %lx\n", uapp_mem);
        memcpy((void *)uapp_mem, _sramdisk, sz);
        printk("memcpy done\n");
        uint64 pa = uapp_mem - PA2VA_OFFSET;
        create_mapping((uint64 *)pgd, va, pa, sz, perm);
        printk("Set %d mapping done\n", i);

        // 设置用户态栈
        printk("Set %d stack\n", i);
        uint64 user_stack = (uint64)alloc_page();
        va = USER_END - PGSIZE;
        pa = user_stack - PA2VA_OFFSET;
        create_mapping((uint64 *)pgd, va, pa, PGSIZE, perm);

        // 设置页表
        task_ptr->pgd = (pagetable_t)(pgd - PA2VA_OFFSET);
        printk("Set %d pgd done, pgd_va = %lx, pgd_pa = %lx\n", i, pgd, task_ptr->pgd);

        task[i] = task_ptr;
    }

    for (int i = 1; i < NR_TASKS; i++) {
    #ifdef SJF
        printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
    #else
        printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
    #endif
    }

    printk("...proc_init done!\n");
}

// arch/riscv/kernel/proc.c
void dummy() {
    printk("dummy\n");
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
        }
    }
}


void switch_to(struct task_struct* next) {
    // 1. 判断下一个执行的线程 next 与当前的线程 current 是否为同一个线程，如果是同一个线程，则无需做任何处理，否则调用 __switch_to 进行线程切换。 

    /* YOUR CODE HERE */
    if (next != current) {
        struct task_struct* prev = current;
        current = next;
        printk("switch to [PID = %d, COUNTER = %d, PRIORITY = %d]\n", current->pid, current->counter, current->priority);
        __switch_to(prev, next);
    }
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    /* YOUR CODE HERE */
    if (current == idle) {
        schedule();
    } else {
        /* YOUR CODE HERE */
        /*
        // 因为在 dummy 中已经对 counter 进行了脏的修改，所以这里不再应该先对 counter 先进行减 1 操作，再进行判断
        current->counter--;
        if (current->counter == 0) {
            schedule();
        }
        */
        if (current->counter == 0 || --(current->counter) == 0) {
            schedule();
        }
    }
}

#ifdef SJF
// 短作业优先调度算法
void schedule(void) {
    /* YOUR CODE HERE */
    struct task_struct ** task_start = &task[0];
    struct task_struct ** p;
    int next;
    uint64 counter;

    while (1) {
        p = task_start;
        next = NR_TASKS;
        counter = -1;
        for (int i = 1; i < NR_TASKS; ++i) {
            if ((*++p) && (*p)->state == TASK_RUNNING && (*p)->counter > 0 && (*p)->counter < counter) {
                counter = (*p)->counter;
                next = i;
            }
        }

        // if no replace happens, means all running tasks have counter = 0
        if (counter != -1) {
            break;
        }

        // ignore idle since p is now pointing to task[1]
        for ( ; p > task_start; --p) {
            if (*p) {
                (*p)->counter = rand() % 13 + 1;
                printk("SET [PID = %d COUNTER = %d]\n", (*p)->pid, (*p)->counter);
            }
        }
    }

    switch_to(task[next]);
}
#else
// 优先级调度算法
void schedule(void) {
    /* YOUR CODE HERE */
    struct task_struct ** task_end = &task[NR_TASKS];
    struct task_struct ** p;
    int next;
    uint64 counter;

    while (1) {
        p = task_end;
        next = NR_TASKS;
        counter = 0;
        for (int i = NR_TASKS-1; i > 0; --i) {
            if ((*--p) && (*p)->state == TASK_RUNNING && (*p)->counter > counter) {
                counter = (*p)->counter;
                next = i;
            }
        }

        if (counter > 0) {
            break;
        }

        // ignore idle since p is now pointing to task[1]
        for ( ; p < task_end; ++p) {
            if (*p) {
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", (*p)->pid, (*p)->priority, (*p)->counter);
            }
        }
    }

    switch_to(task[next]);
}
#endif
