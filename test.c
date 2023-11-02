// 位域内存测试
#include <stdio.h>
#include <stddef.h>
/* 用于记录 `线程` 的 `内核栈与用户栈指针` */
/* (lab2 中无需考虑, 在这里引入是为了之后实验的使用) */
typedef unsigned long uint64;

struct thread_info {
    uint64 kernel_sp;
    uint64 user_sp;
};

/* 线程状态段数据结构 */
struct thread_struct {
    uint64 ra;
    uint64 sp;
    uint64 s[12];
};

/* 线程数据结构 */
struct task_struct {
    struct thread_info thread_info;
    uint64 state;    // 线程状态
    uint64 counter;  // 运行剩余时间
    uint64 priority; // 运行优先级 1最低 10最高
    uint64 pid;      // 线程id

    struct thread_struct thread;
};

void test(void) {
    printf("test\n");
}

int main(int argc, char const *argv[])
{
    struct task_struct task;
    printf("task_struct size: %lu\n", sizeof(task));
    printf("offset of thread: %lu\n", offsetof(struct task_struct, thread));
    // store 1-12 to s[0]-s[11]
    for (int i = 0; i < 12; i++) {
        *(uint64 *)((char *)(&task) + 64 + i * 8) = i + 1;
    }
    // print s[0]-s[11]
    for (int i = 0; i < 12; i++) {
        printf("s[%d] = %lu\n", i, task.thread.s[i]);
    }
    test();
    struct task_struct *task_ptr = &task;
    printf("*(char *)task_ptr = %d\n", *(char *)task_ptr);
    task_ptr->thread.ra = (uint64)test;
    printf("*(char *)task_ptr = %d\n",  *(char *)task_ptr);
    return 0;
}
