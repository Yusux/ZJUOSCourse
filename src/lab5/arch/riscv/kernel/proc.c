//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"

//arch/riscv/kernel/proc.c

extern void __switch_to(struct task_struct *prev, struct task_struct *next);
extern uint64_t load_program(struct task_struct *task);

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

void task_init() {
    test_init(NR_TASKS);
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


    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    /* YOUR CODE HERE */
    for (int i = 1; i < NR_TASKS; i++) {
        struct task_struct *task_ptr = (struct task_struct*)kalloc();
        task_ptr->state = TASK_RUNNING;
        task_ptr->counter = task_test_counter[i];
        task_ptr->priority = task_test_priority[i];
        task_ptr->pid = i;

        if (load_program(task_ptr) == -1) {
            printk("Load program failed\n");
            return;
        }

        task[i] = task_ptr;
    }

    for (int i = 1; i < NR_TASKS; i++) {
    #ifdef SJF
        printk("[S-MODE] SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
    #else
        printk("[S-MODE] SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
    #endif
    }

    printk("...proc_init done!\n");
}

void switch_to(struct task_struct* next) {
    // 1. 判断下一个执行的线程 next 与当前的线程 current 是否为同一个线程，如果是同一个线程，则无需做任何处理，否则调用 __switch_to 进行线程切换。 

    /* YOUR CODE HERE */
    if (next != current) {
        struct task_struct *prev = current;
        current = next;
        printk("[S-MODE] switch to [PID = %d, PRIORITY = %d, COUNTER = %d]\n", current->pid, current->priority, current->counter);
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
    struct task_struct **task_start = &task[0];
    struct task_struct **p;
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
                printk("[S-MODE] SET [PID = %d COUNTER = %d]\n", (*p)->pid, (*p)->counter);
            }
        }
    }

    switch_to(task[next]);
}
#else
// 优先级调度算法
void schedule(void) {
    /* YOUR CODE HERE */
    struct task_struct **task_end = &task[NR_TASKS];
    struct task_struct **p;
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
                printk("[S-MODE] SET [PID = %d PRIORITY = %d COUNTER = %d]\n", (*p)->pid, (*p)->priority, (*p)->counter);
            }
        }
    }

    switch_to(task[next]);
}
#endif

void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file) {
    // 为 task 指定一个新的 vma，
    // 通过将 vmas 后的区域向后移动一个 vma 的大小来实现
    struct vm_area_struct *vma = &task->vmas[task->vma_cnt++];

    // 设置 vma 的各个域
    uint64_t start_page_addr = PGROUNDDOWN(addr);
    uint64_t end_page_addr = PGROUNDUP(addr + length);

    vma->vm_start = start_page_addr;
    vma->vm_end = end_page_addr;
    vma->vm_flags = flags;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr) {
    // 根据 vma_cnt 所记录的大小，遍历 task 的 vma 数组
    // 如果找到了包含 addr 的 vma，则返回该 vma，否则返回 NULL
    for (int i = 0; i < task->vma_cnt; i++) {
        // 由于内存映射的地址空间是连续的，所以只需要判断 addr
        // 是否在 vma 的地址空间 [vm_start, vm_end) 中即可
        if (task->vmas[i].vm_start <= addr && addr < task->vmas[i].vm_end) {
            return &task->vmas[i];
        }
    }
    return NULL;
}

uint64_t trans_vm_flags(uint64_t vm_flags) {
    // 将 VMA 的 flags 转换为 PTE 的 flags
    // 排除最低位的 VM_ANONYM 标志位
    return vm_flags & 0xe;
}
