#include <stdio.h>
#include <unistd.h>

int gval = 10;

int main(int argc, char const *argv[])
{
    pid_t pid;
    int lval = 20;
    gval++, lval += 5;                      // gval:11，lval:25

    // 创建进程，复制父进行的内存
    pid = fork();

    // 判断执行子进程，还是父进程
    if (pid == 0)
        gval += 2, lval += 2;
    else
        gval -= 2, lval -= 2;

    if (pid == 0)
        printf("Child Proc: [%d, %d]\n", gval, lval);
    else
        printf("Parent Proc: [%d, %d]\n", gval, lval);

    return 0;
}
