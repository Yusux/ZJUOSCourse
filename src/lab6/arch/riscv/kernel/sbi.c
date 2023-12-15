#include "types.h"
#include "sbi.h"

struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
    struct sbiret ret;
    ret.error = 0;
    ret.value = 0;
    // According to https://stackoverflow.com/questions/67860150/how-to-specify-a-specific-register-to-assign-the-result-of-a-c-expression-in-inl
    // to determine which register to use
    register uint64 a0 asm("a0") = arg0;
    register uint64 a1 asm("a1") = arg1;
    register uint64 a2 asm("a2") = arg2;
    register uint64 a3 asm("a3") = arg3;
    register uint64 a4 asm("a4") = arg4;
    register uint64 a5 asm("a5") = arg5;
    register uint64 a6 asm("a6") = fid;
    register uint64 a7 asm("a7") = ext;
    asm volatile (
        "ecall\n"
        "mv %[error], a0\n"
        "mv %[value], a1\n"
        : [error] "=r" (ret.error), [value] "=r" (ret.value)
        : "r" (a0), "r" (a1), "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
        : "memory"
    );
    return ret;
}
