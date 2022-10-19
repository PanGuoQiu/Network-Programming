#include <stdio.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    pid_t pid = fork();

    // 判断进程
    if (pid == 0)
    {
        puts("Hi, I am a child Process");
    }
    else
    {
        printf("Child Process ID: %d \n", pid);
        sleep(30);
    }

    if (pid)
        puts("End child process");
    else
        puts("End parent process");

    return 0;
}
