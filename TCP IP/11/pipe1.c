#include <stdio.h>
#include <unistd.h>

#define BUF_SIZE 30

int main(int argc, char const *argv[])
{
    int fds[2];
    char str[] = "Who are you?";
    char buf[BUF_SIZE];
    pid_t pid;

    // 调用pipe函数创建管道，fds数组中保存用于I/O的文件描述符
    pipe(fds);

    // 创建子进程：将同时拥有创建管道获取的2个文件描述符，复制的并非管道，而是文件描述符
    pid = fork();
    if (pid == 0)
    {
        write(fds[1], str, sizeof(str));                // 管道1：写端
    }
    else
    {
        read(fds[0], buf, BUF_SIZE);                    // 管道2：读端
        puts(buf);
    }

    return 0;
}
