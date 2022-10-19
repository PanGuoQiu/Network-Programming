#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// 信号处理函数
void timeout(int sig)
{
    // 判断信号的类型
    if (sig == SIGALRM)
        puts("Time out");

    // 为了每隔2秒重复产生SLGALRM信号，在信号处理器中调用alarm函数
    alarm(2);
}

// 信号处理函数
void keycontrol(int sig)
{
    if (sig == SIGINT)
        puts("CTRL+C pressed");
}

int main(int argc, char const *argv[])
{
    int i;

    // 注册信号及相应的处理器
    signal(SIGALRM, timeout);
    signal(SIGINT, keycontrol);

    // 预约2秒后，发生SIGALRM信号
    alarm(2);

    for (i = 0; i < 3; i++)
    {
        puts("wait...");
        sleep(100);
    }

    return 0;
}
