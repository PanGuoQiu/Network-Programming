#include <stdio.h>
#include <fcntl.h>

int main(int argc, char const *argv[])
{
    FILE *fp;
    int fd = open("2.dat", O_WRONLY | O_CREAT | O_TRUNC);    // 创建文件并返回文件描述符
    if (fd == -1)
    {
        fputs("file open error\n", stdout);
        return -1;
    }

    fp = fdopen(fd, "w");                                       // 返回写模式的FILE指针
    fputs("NetWork C programming\n", fp);

    fclose(fp);
    return 0;
}
