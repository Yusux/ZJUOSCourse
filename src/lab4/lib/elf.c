#include "elf.h"
#include "mm.h"
#include "defs.h"
#include "string.h"
#include "proc.h"
#include "printk.h"

extern unsigned long swapper_pg_dir[];  // kernel pagetable root, mapped in setup_vm_final
extern char _sramdisk[];                // start address of the ELF file in memory
extern char _eramdisk[];                // end address of the ELF file in memory

extern void __dummy();
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

uint64_t trans_p_flags(Elf64_Word p_flags) {
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
        return -1;
    }

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr *phdr;
    int load_phdr_cnt = 0;

    // set up page table for user program
    uint64 pgd = (uint64)alloc_page();
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
            // alloc space and copy content
            uint64 start_vpg = PGROUNDDOWN(phdr->p_vaddr);
            uint64 end_vpg = PGROUNDUP(phdr->p_vaddr + phdr->p_memsz);
            uint64 start_offset = phdr->p_vaddr - start_vpg;
            uint64 pg_num = (end_vpg - start_vpg) / PGSIZE;
            uint64 perm = trans_p_flags(phdr->p_flags) | PTE_V | PTE_U;
            uint64 uapp_mem = alloc_pages(pg_num);
            // copy content
            memcpy((void *)(uapp_mem + start_offset), (void *)(_sramdisk + phdr->p_offset), phdr->p_filesz);
            // clear [p_vaddr + p_filesz, p_vaddr + p_memsz) for .bss section
            memset((void *)(uapp_mem + start_offset + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            // do mapping
            uint64 va = start_vpg;
            uint64 pa = uapp_mem - PA2VA_OFFSET;
            create_mapping((uint64 *)pgd, start_vpg, pa, pg_num * PGSIZE, perm);
        }
    }

    // allocate user stack and do mapping
    uint64 user_stack = (uint64)alloc_page();
    uint64 va = USER_END - PGSIZE;
    uint64 pa = user_stack - PA2VA_OFFSET;
    create_mapping((uint64 *)pgd, va, pa, PGSIZE, PTE_V | PTE_R | PTE_W | PTE_U);

    // following code has been written for you
    // set ra for the user program
    task->thread.ra = (uint64)__dummy;
    // set user stack
    task->thread.sp = (uint64)task + PGSIZE;
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    task->thread.sstatus = (SSTATUS_SUM | SSTATUS_SPIE) & (~SSTATUS_SPP);
    // user stack for user program
    task->thread.sscratch = USER_END;

    return load_phdr_cnt;
}