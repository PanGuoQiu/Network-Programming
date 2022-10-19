#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int main(int argc, char const *argv[])
{
    int sock;
    char buf[BUF_SIZE];
    struct sockaddr_in serv_addr;

    FILE *readfp;
    FILE *writefp;

    // 创建套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    // 设置IP地址和端口号
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 连接请求
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    // 转换文件描述符为文件指针
    readfp = fdopen(sock, "r");
    writefp = fdopen(sock, "w");

    while (1)
    {
        if (fgets(buf, sizeof(buf), readfp) == NULL)
            break;

        fputs(buf, stdout);
        fflush(stdout);
    }

    fputs("FROM CLIENT: Thank you \n", writefp);
    fflush(writefp);
    fclose(writefp);
    fclose(readfp);

    return 0;
}
