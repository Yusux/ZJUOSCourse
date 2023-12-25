#include "elf.h"
#include "mm.h"
#include "defs.h"
#include "string.h"
#include "proc.h"
#include "printk.h"

extern unsigned long swapper_pg_dir[];  // kernel pagetable root, mapped in setup_vm_final
extern char _sramdisk[];                // start address of the ELF file in memory

extern void __dummy();

uint64_t trans_p_flags(Elf64_Word p_flags) {
    // convert ELF flags to PTE/VM XWR flags
    uint64_t perm = 0;
    if (p_flags & P_FLAGS_R) {
        perm |= PTE_R;
    }
    if (p_flags & P_FLAGS_W) {
        perm |= PTE_W;
    }
    if (p_flags & P_FLAGS_X) {
        perm |= PTE_X;
    }
    return perm;
}

uint64_t load_program(struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)_sramdisk;

    // check magic number
    if (!(ehdr->e_ident[0]  == 0x7f &&
          ehdr->e_ident[1]  == 0x45 &&
          ehdr->e_ident[2]  == 0x4c &&
          ehdr->e_ident[3]  == 0x46 &&
          ehdr->e_ident[4]  == 0x02 &&
          ehdr->e_ident[5]  == 0x01 &&
          ehdr->e_ident[6]  == 0x01 &&
          ehdr->e_ident[7]  == 0x00 &&
          ehdr->e_ident[8]  == 0x00 &&
          ehdr->e_ident[9]  == 0x00 &&
          ehdr->e_ident[10] == 0x00 &&
          ehdr->e_ident[11] == 0x00 &&
          ehdr->e_ident[12] == 0x00 &&
          ehdr->e_ident[13] == 0x00 &&
          ehdr->e_ident[14] == 0x00 &&
          ehdr->e_ident[15] == 0x00)) {
        printk("Not a valid elf file\n");
        printk("e_ident[0-15]:\
                %x %x %x %x %x %x %x %x\n\
                %x %x %x %x %x %x %x %x\n",
                ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3],
                ehdr->e_ident[4], ehdr->e_ident[5], ehdr->e_ident[6], ehdr->e_ident[7],
                ehdr->e_ident[8], ehdr->e_ident[9], ehdr->e_ident[10], ehdr->e_ident[11],
                ehdr->e_ident[12], ehdr->e_ident[13], ehdr->e_ident[14], ehdr->e_ident[15]);
        return -1;
    }

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr *phdr;
    int load_phdr_cnt = 0;

    // set up page table for user program
    uint64_t pgd = (uint64_t)alloc_page();
    // set up task->pgd
    task->pgd = (pagetable_t)(pgd - PA2VA_OFFSET);
    // in order to avoid switching page table
    // when switching between U-Mode and S-Mode,
    // we copy the kernel page table (swapper_pg_dir)
    // to the page table of each process
    memcpy((void *)pgd, swapper_pg_dir, PGSIZE);

    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            load_phdr_cnt++;

            // create mmap for the PT_LOAD segment
            // it is not an anonymous mmap, so we only need to consider X/W/R
            uint64_t vm_flags = trans_p_flags(phdr->p_flags);
            do_mmap(task, phdr->p_vaddr, phdr->p_memsz, vm_flags, phdr->p_offset, phdr->p_filesz);
        }
    }

    // create mmap for the stack
    do_mmap(task, USER_END - PGSIZE, PGSIZE, VM_ANONYM | VM_R_MASK | VM_W_MASK, 0, 0);

    // following code has been written for you
    // set ra for the user program
    task->thread.ra = (uint64_t)__dummy;
    // set user stack
    task->thread.sp = (uint64_t)task + PGSIZE;
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    task->thread.sstatus = (SSTATUS_SUM | SSTATUS_SPIE) & (~SSTATUS_SPP);
    // user stack for user program
    task->thread.sscratch = USER_END;

    return load_phdr_cnt;
}