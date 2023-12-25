#include "syscall.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "mm.h"

extern struct task_struct *current;
extern struct task_struct *task[NR_TASKS];  // 线程数组, 所有的线程都保存在此
extern uint64_t task_test_priority[];       // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64_t task_test_counter[];        // test_init 后, 用于初始化 task[i].counter  的数组
extern unsigned long swapper_pg_dir[];      // kernel pagetable root, mapped in setup_vm_final

extern void __ret_from_fork();
extern uint64_t walk_page_table(uint64_t *pgtbl, uint64_t va, uint64_t is_create);
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

// 实现 sys_read
int64_t sys_read(unsigned int fd, char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        // check if the file is readable
        if (target_file->perms & FILE_READABLE) {
            target_file->read(target_file, buf, count);
            ret = count;
        } else {
            printk("file not readable\n");
            ret = ERROR_FILE_NOT_OPEN;
        }
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

// 实现 sys_write
int64_t sys_write(unsigned int fd, const char *buf, size_t count) {
    int64_t ret;
    // get current file struct
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        // check if the file is writable
        if (target_file->perms & FILE_WRITABLE) {
            target_file->write(target_file, buf, count);
            ret = count;
        } else {
            printk("file not writable\n");
            ret = ERROR_FILE_NOT_OPEN;
        }
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

// 实现 sys_getpid
uint64_t sys_getpid() {
    // 返回当前线程的 pid
    return current->pid;
}

// 实现 sys_clone
uint64_t sys_clone(struct pt_regs *regs) {
    /*
     * 在完全拷贝 task_struct 的情况下，
     * 需要考虑如下四个修改
     * 1. task_struct 的 pid 等线程信息
     * 2. child task 的地址
     *   2.1 由于用户态进程空间的地址都是虚拟地址
     *       只需要在第 4 点拷贝页表即可
     *   2.2 其他的地址，例如 ra 返回地址、sp 栈指针，
     *       以及在栈中储存等待恢复的 x[2](sp)、
     *       用于异常返回下一条指令的 sepc 都需要进行修改
     * 3. child task 的返回值 (regs->x[10]) 没有办法
     *    在 tarp_handler 中赋值，需要手动赋值 0
     * 4. child task 的页表需要在对内核页表 swapper_pg_dir
     *    进行浅拷贝的基础上，再对 parent task 的
     *    用户态页表 (由 vma 管理) 进行深拷贝来实现复制
     */

    // 检查是否有空闲的线程可供创建
    int child_pid = -1;
    // 从 1 开始以忽略 idle 线程
    for (int i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL) {
            child_pid = i;
            break;
        }
    }
    // 如果没有空闲的线程可供创建，则返回 -1
    if (child_pid == -1) {
        return -1;
    }

    // 为 child task 分配一个 task_struct
    struct task_struct *new_task = (struct task_struct*)kalloc();
    // 将 parent task 的 task_struct 复制到 child task 的 task_struct
    memcpy(new_task, current, PGSIZE);
    // 为后续操作提供到 parent task 的偏移量
    uint64_t offset_to_parent = (uint64_t)new_task - (uint64_t)current;

    // 设置 child task 的计数器、优先级、pid
    new_task->counter = task_test_counter[child_pid];
    new_task->priority = task_test_priority[child_pid];
    new_task->pid = child_pid;

    // 设置 child task 的 ra 和 sp
    new_task->thread.ra = (uint64_t)__ret_from_fork;
    // 根据到 parent task 的偏移量设置 child task 的 sp
    new_task->thread.sp = regs->x[2] - 296 + offset_to_parent;

    // 根据 regs 计算出 child task 的 pt_regs 的地址
    struct pt_regs *new_regs = (struct pt_regs*)((uint64_t)regs + offset_to_parent);
    // 设置 child task 的 a0、sp、sepc
    new_regs->x[10] = 0;
    // 之所以需要设置 sp，是因为从 trap 返回时，sp 会被恢复为 regs->x[2]
    new_regs->x[2] = new_task->thread.sp + 296;
    // 执行 ecall 的下一条指令
    new_regs->sepc = regs->sepc + 4;

    // 为 child task 申请 pgd
    uint64_t pgd = (uint64_t)alloc_page();
    // 设置 child task 的 pgd
    new_task->pgd = (pagetable_t)(pgd - PA2VA_OFFSET);
    // 拷贝内核页表 swapper_pg_dir 来初始化 child task 的 pgd
    // 不能拷贝 parent task 的 pgd，否则会导致 parent task 和
    // child task 因为一二级已存在的页表项映射到同一个物理页
    // 而导致的页表项发生共享
    memcpy((void *)pgd, (void *)swapper_pg_dir, PGSIZE);

    // 为 child task 深拷贝用户态进程空间
    // 获取 parent task 的 pgd
    uint64_t parent_pgd = (uint64_t)current->pgd + PA2VA_OFFSET;
    // 遍历 vma
    for (int i = 0; i < new_task->vma_cnt; i++) {
        struct vm_area_struct *vma = &(new_task->vmas[i]);
        uint64_t va = vma->vm_start;
        uint64_t vm_end = vma->vm_end;
        uint64_t perm = trans_vm_flags(vma->vm_flags) | PTE_V | PTE_U;
        // 遍历 vma 中的每一页
        while (va < vm_end) {
            // 检查是否需要创建页表
            uint64_t parent_pte_addr = walk_page_table((uint64_t *)parent_pgd, va, 0);
            if (parent_pte_addr != 0x1) {
                // printk("sys_clone: find parent pte to copy, page range: [%lx, %lx]\n", va, va + PGSIZE);
                // 如果在 parent task 的页表中找到了对应的页表项
                // 则深拷贝这一页
                // 获取 parent task 中的页表项代表的物理页的虚拟地址
                uint64_t parent_page = PTE2PA(*(uint64_t *)parent_pte_addr) + PA2VA_OFFSET;
                // 为 child task 分配一个物理页
                uint64_t child_page = (uint64_t)alloc_page();
                // 拷贝 parent task 中的页到 child task 中
                memcpy((void *)child_page, (void *)parent_page, PGSIZE);
                // 将 child task 的页映射到对应的用户地址空间
                create_mapping((uint64_t *)pgd, va, child_page - PA2VA_OFFSET, PGSIZE, perm);
            }
            // 更新 va
            va += PGSIZE;
        }
    }

    // 将 child task 加入线程数组
    task[child_pid] = new_task;

    // 返回子 task 的 pid
    return child_pid;
}

// 实现 syscall 入口
void syscall(struct pt_regs *regs) {
    int return_value = 0;
    switch (regs->x[17]) {
        case SYS_READ:
            return_value = sys_read(regs->x[10], (char*)regs->x[11], regs->x[12]);
            break;
        case SYS_WRITE:
            return_value = sys_write(regs->x[10], (const char*)regs->x[11], regs->x[12]);
            break;
        case SYS_GETPID:
            return_value = sys_getpid();
            break;
        case SYS_CLONE:
            return_value = sys_clone(regs);
            break;
        default:
            break;
    }
    regs->x[10] = return_value;
    regs->sepc += 4;
}