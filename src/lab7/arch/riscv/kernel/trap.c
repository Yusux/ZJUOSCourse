// trap.c
#include "trap.h"
#include "printk.h"
#include "syscall.h"
#include "string.h"
#include "defs.h"
#include "mm.h"
#include "proc.h"

extern struct task_struct *current; // 当前线程 task_struct
extern char _sramdisk[];            // ELF 在内存中的起始地址

extern void do_timer(void);
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

void do_page_fault(struct pt_regs *regs) {
    // 获得访问出错的虚拟内存地址
    uint64_t bad_addr = regs->stval;

    // 查找 Bad Address 是否在某个 vma 中
    struct vm_area_struct *vma = find_vma(current, bad_addr);

    // 如果没有找到 vma
    if (vma == NULL) {
        printk("[S-MODE] Page fault at %lx, badaddr is %lx\n", regs->sepc, bad_addr);
        printk("[S-MODE] Cannot find vma\n");
        while (1);
    } else { // 如果找到了 vma
        // 分配一个页
        uint64_t page = (uint64_t)alloc_page();
        memset((void *)page, 0, PGSIZE);
        
        // 如果不是匿名空间，考虑是否需要拷贝 content
        if (!(vma->vm_flags & VM_ANONYM)) {
            // 计算 bad_addr 所在的页相对于文件起始位置的偏移量
            uint64_t bad_page_start_offset = PGROUNDDOWN(bad_addr) - vma->vm_start;
            uint64_t bad_page_end_offset = bad_page_start_offset + PGSIZE;
            // 计算有效的 content 相对于文件起始位置的偏移量
            uint64_t content_start_offset = vma->vm_content_offset_in_file;
            uint64_t content_end_offset = content_start_offset + vma->vm_content_size_in_file;

            // 如果 bad_addr 所在的页与 content 有重叠
            if (bad_page_start_offset < content_end_offset && bad_page_end_offset > content_start_offset) {
                // 计算重叠部分的起始地址、结束地址的偏移量，及重叠部分的大小
                uint64_t overlap_start_offset = max(bad_page_start_offset, content_start_offset);
                uint64_t overlap_end_offset = min(bad_page_end_offset, content_end_offset);
                uint64_t overlap_size = overlap_end_offset - overlap_start_offset;

                // 计算重叠部分在 bad_addr 所在页中的偏移量
                uint64_t overlap_page_offset = overlap_start_offset - bad_page_start_offset;
                // 根据上述偏移量，计算拷贝到 page 中的目的地址
                uint64_t page_dst_addr = page + overlap_page_offset;

                // 根据文件起始位置和重叠部分的偏移量，计算重叠部分在内存中的地址
                uint64_t file_src_addr = (uint64_t)_sramdisk + overlap_start_offset;

                // 拷贝重叠部分的内容
                memcpy((void *)page_dst_addr, (void *)file_src_addr, overlap_size);
            }
        }

        // 将这个页映射到对应的用户地址空间
        uint64_t pgd = (uint64_t)current->pgd + PA2VA_OFFSET;
        uint64_t va = PGROUNDDOWN(bad_addr);
        uint64_t pa = page - PA2VA_OFFSET;
        uint64_t perm = trans_vm_flags(vma->vm_flags) | PTE_V | PTE_U;
        create_mapping((uint64 *)pgd, va, pa, PGSIZE, perm);
    }
}

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
    // if it is an interrupt
    if (scause >> 63) {
        if ((scause & 0xff) == 0x5) {
            // Timer interrupt occurs, set next timer interrupt
            clock_set_next_event();
            // Call do_timer() to update task status
            do_timer();
            // Print time interrupt info
            // printk("[S] Supervisor Mode Timer Interrupt\n");
            return;
        }
    } else { // if it is an exception
        if ((scause & 0xff) == 0x8) {
            // Environment call from U-mode
            syscall(regs);
            return;
        } else if ((scause & 0xff) == 0xc ||
                   (scause & 0xff) == 0xd ||
                   (scause & 0xff) == 0xf) {
            // Page fault
            do_page_fault(regs);
            return;
        }
    }

    printk("[S-MODE] Unhandled trap, ");
    printk("scause: %lx, ", scause);
    printk("stval: %lx, ", regs->stval);
    printk("sepc: %lx\n", regs->sepc);
    while (1);
}