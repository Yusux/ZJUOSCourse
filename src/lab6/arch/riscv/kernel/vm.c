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
    uint64_t pa = PHY_START;
    uint64_t va = VM_START;    // pa + PA2VA_OFFSET
    // 等值映射
    uint64_t index = VPN2(pa);
    early_pgtbl[index] = (PPN2(pa) << 28) | PTE_V | PTE_R | PTE_W | PTE_X;
    // 映射到 direct mapping area
    index = VPN2(va);
    early_pgtbl[index] = (PPN2(pa) << 28) | PTE_V | PTE_R | PTE_W | PTE_X;
}

uint64_t walk_page_table(uint64_t *pgtbl, uint64_t va, uint64_t is_create) {
    /*
    用于在三级页表中查找 va 对应的 page table entry
    1. 在 is_create 为 1 的情况下，
       若一二级页表的 entry 不存在，分配一个页作为 entry
       最终返回三级页表的 entry 地址
    2. 在 is_create 为 0 的情况下，
       若找到，返回该三级页表的 entry
       否则返回 0x1
       （因为 0x1 不可能是一个合法的三级页表 entry 的地址）
    */
    uint64_t *current_pgtbl, *current_pte, current_index;

    // first level page table
    current_pgtbl = pgtbl;                          // get root page table
    current_index = VPN2(va);                       // get index of root page table
    current_pte = &current_pgtbl[current_index];    // get page table entry
    
    if ((*current_pte & PTE_V) == 0) {  // check if page table entry is valid
        // if not valid
        if (is_create) {
            // if is_create, allocate a page for page table
            uint64_t *new_pgtbl = (uint64_t *)(kalloc() - PA2VA_OFFSET);            // get physical address of new page table
            *current_pte = (PPN((uint64_t)new_pgtbl) << 10) | PTE_V;
        } else {
            // if not is_create, return 0x1
            return 0x1;
        }
    }

    // second level page table
    current_pgtbl = (uint64_t *)(((*current_pte >> 10) << 12) + PA2VA_OFFSET);    // get second level page table
    current_index = VPN1(va);                                                   // get index of second level page table
    current_pte = &current_pgtbl[current_index];                                // get page table entry

    if ((*current_pte & PTE_V) == 0) {  // check if page table entry is valid
        // if not valid
        if (is_create) {
            // if is_create, allocate a page for page table
            uint64_t *new_pgtbl = (uint64_t *)(kalloc() - PA2VA_OFFSET);            // get physical address of new page table
            *current_pte = (PPN((uint64_t)new_pgtbl) << 10) | PTE_V;
        } else {
            // if not is_create, return 0x1
            return 0x1;
        }
    }

    // third level page table
    current_pgtbl = (uint64_t *)(((*current_pte >> 10) << 12) + PA2VA_OFFSET);    // get third level page table
    current_index = VPN0(va);                                                   // get index of third level page table
    current_pte = &current_pgtbl[current_index];                                // get page table entry

    // if not is_create and page table entry is not valid, return 0x1
    if (!is_create && ((*current_pte & PTE_V) == 0)) {
        return 0x1;
    }

    return (uint64_t)current_pte;
}

/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm) {
    /*
    pgtbl 为根页表的基地址，是虚拟地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小，单位为字节
    perm 为映射的权限 (即页表项的低 8 位)

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    uint64_t pa_end = pa + sz;
    uint64_t *current_pte;
    while (pa < pa_end) {
        // get page table entry
        current_pte = (uint64_t *)walk_page_table(pgtbl, va, 1);
        *current_pte = (PPN(pa) << 10) | perm | PTE_V;                          // cover page table entry

        // allocated a page, add the PGSIZE
        va += PGSIZE;
        pa += PGSIZE;
    }
}

void setup_vm_final(void) {
    uint64_t stext = (uint64_t)_stext - PA2VA_OFFSET;
    uint64_t srodata = (uint64_t)_srodata - PA2VA_OFFSET;
    uint64_t sdata = (uint64_t)_sdata - PA2VA_OFFSET;
    uint64_t va = VM_START;

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
    uint64_t satp_val = (8L << 60) | PPN((uint64_t)swapper_pg_dir - PA2VA_OFFSET);
    csr_write(satp, satp_val);

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");
    
    /*
    // check if .text and .rodata has R permission
    printk("The value of _stext is %lx\n", *(uint64_t *)_stext);
    printk("The value of _srodata is %lx\n", *(uint64_t *)_srodata);
    // check if .text and .rodata has W permission
    *(uint64_t *)_stext = 0xdeadbeef;
    *(uint64_t *)_srodata = 0xdeadbeef;
    */

    return;
}