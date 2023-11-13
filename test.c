// 位域内存测试
#include <stdio.h>
#include <stddef.h>
unsigned long  early_pgtbl[512] __attribute__((__aligned__(0x1000)));
unsigned long  a;

int main(int argc, char const *argv[])
{
    printf("Address of early_pgtbl: %p\n", early_pgtbl);
    printf("Address of early_pgtbl[0]: %p\n", &early_pgtbl[0]);
    printf("Address of early_pgtbl[1]: %p\n", &early_pgtbl[1]);
    printf("Address of early_pgtbl[511]: %p\n", &early_pgtbl[511]);
    printf("Size of early_pgtbl: %lu\n", sizeof(early_pgtbl));
    printf("Size of early_pgtbl[0]: %lu\n", sizeof(early_pgtbl[0]));
    printf("Address of a: %p\n", &a);
}
