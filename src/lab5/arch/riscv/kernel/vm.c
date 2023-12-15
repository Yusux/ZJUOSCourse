#include "defs.h"
#include "mm.h"
#include "string.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long  early_pgtbl[512] __attribute__((__aligned__(0x1000)));
/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern char _stext[];
extern char _srodata[];
extern char _sdata[];

void setup_vm(void) {
    /* 
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表 
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间 9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。 
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    memset(early_pgtbl, 0x0, PGSIZE);
    uint64 pa = PHY_START;
    uint64 va = VM_START;    // pa + PA2VA_OFFSET
    // 等值映射
    uint64 index = VPN2(pa);
    early_pgtbl[index] = (PPN2(pa) << 28) | PTE_V | PTE_R | PTE_W | PTE_X;
    // 映射到 direct mapping area
    index = VPN2(va);
    early_pgtbl[index] = (PPN2(pa) << 28) | PTE_V | PTE_R | PTE_W | PTE_X;
}

/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
    /*
    pgtbl 为根页表的基地址，是虚拟地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小，单位为字节
    perm 为映射的权限 (即页表项的低 8 位)

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    uint64 pa_end = pa + sz;
    uint64 *current_pgtbl, *current_pte, current_index;
    while (pa < pa_end) {
        // first level page table
        current_pgtbl = pgtbl;                          // get root page table
        current_index = VPN2(va);                       // get index of root page table
        current_pte = &current_pgtbl[current_index];    // get page table entry
        
        if ((*current_pte & PTE_V) == 0) {  // check if page table entry is valid
            // if not valid, allocate a page for page table
            uint64 *new_pgtbl = (uint64 *)(kalloc() - PA2VA_OFFSET);    // get physical address of new page table
            *current_pte = (PPN((uint64)new_pgtbl) << 10) | PTE_V;
        }

        // second level page table
        current_pgtbl = (uint64 *)(((*current_pte >> 10) << 12) + PA2VA_OFFSET);    // get second level page table
        current_index = VPN1(va);                                                   // get index of second level page table
        current_pte = &current_pgtbl[current_index];                                // get page table entry

        if ((*current_pte & PTE_V) == 0) {  // check if page table entry is valid
            // if not valid, allocate a page for page table
            uint64 *new_pgtbl = (uint64 *)(kalloc() - PA2VA_OFFSET);    // get physical address of new page table
            *current_pte = (PPN((uint64)new_pgtbl) << 10) | PTE_V;
        }

        // third level page table
        current_pgtbl = (uint64 *)(((*current_pte >> 10) << 12) + PA2VA_OFFSET);    // get third level page table
        current_index = VPN0(va);                                                   // get index of third level page table
        current_pte = &current_pgtbl[current_index];                                // get page table entry

        *current_pte = (PPN(pa) << 10) | perm | PTE_V;                          // cover page table entry

        // allocated a page, add the PGSIZE
        va += PGSIZE;
        pa += PGSIZE;
    }
}

void setup_vm_final(void) {
    uint64 stext = (uint64)_stext - PA2VA_OFFSET;
    uint64 srodata = (uint64)_srodata - PA2VA_OFFSET;
    uint64 sdata = (uint64)_sdata - PA2VA_OFFSET;
    uint64 va = VM_START;

    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    va += OPENSBI_SIZE;
    create_mapping(swapper_pg_dir, va, stext, srodata - stext, PTE_R | PTE_X | PTE_V);
    // printk("The virtual address of _stext is %lx\n", va);

    // mapping kernel rodata -|-|R|V
    va += srodata - stext;
    create_mapping(swapper_pg_dir, va, srodata, sdata - srodata, PTE_R | PTE_V);
    // printk("The virtual address of _srodata is %lx\n", va);
    
    // mapping other memory -|W|R|V
    va += sdata - srodata;
    create_mapping(swapper_pg_dir, va, sdata, PHY_END - sdata, PTE_R | PTE_W | PTE_V);

    // set satp with swapper_pg_dir
    uint64 satp_val = (8L << 60) | PPN((uint64)swapper_pg_dir - PA2VA_OFFSET);
    csr_write(satp, satp_val);

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");
    
    /*
    // check if .text and .rodata has R permission
    printk("The value of _stext is %lx\n", *(uint64 *)_stext);
    printk("The value of _srodata is %lx\n", *(uint64 *)_srodata);
    // check if .text and .rodata has W permission
    *(uint64 *)_stext = 0xdeadbeef;
    *(uint64 *)_srodata = 0xdeadbeef;
    */

    return;
}