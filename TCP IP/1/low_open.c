#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// 显示错误信息
void error_handling(char *message);

int main()
{
    int fd;
    char buf[] = "Let's go!\n";

    // O_CREAT | O_WRONLY | T_TRUNC是文件打开模式，将创建新文件，并且只能写。如存在data.txt文件，则清空文件中的全部数据。
    fd = open("data.txt", O_CREAT | O_WRONLY | O_TRUNC);
    if (fd == -1)
        error_handling("open() error!");
    printf("file descriptor: %d \n", fd);

    // 向对应fd中保存的文件描述符的文件传输buf中保存的数据
    if (write(fd, buf, sizeof(buf)) == -1)
        error_handling("write() error!");

    close(fd);
    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
