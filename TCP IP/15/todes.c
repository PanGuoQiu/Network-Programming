#include <stdio.h>
#include <fcntl.h>

int main(int argc, char const *argv[])
{
    FILE *fp;
    int fd = open("3.dat", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1)
    {
        fputs("file open error\n", stdout);
        return -1;
    }

    printf("First file descriptor: %d \n", fd);

    fp = fdopen(fd, "w");                                       // 转成file指针
    fputs("TCP/IP SOCKET PROGRAMMING \n", fp);
    printf("Second file descriptor: %d \n", fileno(fp));        // 转回文件描述符

    fclose(fp);
    return 0;
}
