#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// 信号处理函数
void read_childproc(int sig)
{
    int status;
    pid_t id = waitpid(-1, &status, WNOHANG);
    if (WIFEXITED(status))
    {
        printf("Removed proc id: %d \n", id);               // 子进程的pid
        printf("Child send: %d \n", WEXITSTATUS(status));   // 子进程的返回值
    }
}

int main(int argc, char const *argv[])
{
    pid_t pid;
    struct sigaction act;
    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    // 注册SIGCHLD信号的处理器
    sigaction(SIGCHLD, &act, 0);

    pid = fork();
    // 判断是子进程执行阶段，还是父进程执行阶段
    if (pid == 0)
    {
        puts("Hi I'm child process");
        sleep(10);
        return 12;
    }
    else
    {
        printf("Child proc id: %d\n", pid);
        pid = fork();
        if (pid == 0)
        {
            puts("Hi! I'm child process");
            sleep(10);
            exit(24);
        }
        else
        {
            int i;
            printf("Child proc id: %d \n",  pid);
            for (i = 0; i < 5; i++)
            {
                puts("wait");
                sleep(5);
            }
        }
    }

    return 0;
}
