#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// 超时信号处理函数
void timeout(int sig)
{
    if (sig == SIGALRM)
        puts("Time out!");

    alarm(2);
}

int main(int argc, char const *argv[])
{
    int i;
    struct sigaction act;
    act.sa_handler = timeout;               // 保存函数指针
    sigemptyset(&act.sa_mask);              // 将sa_mask函数的所有位初始化成0
    act.sa_flags = 0;                       // sa_flags同样初始化成0
    // 注册SIGALRM信号的处理器
    sigaction(SIGALRM, &act, 0);

    // 2秒后发生SIGALRM信号
    alarm(2);

    for (i = 0; i < 3; i++)
    {
        puts("wait...");
        sleep(100);
    }

    return 0;
}
