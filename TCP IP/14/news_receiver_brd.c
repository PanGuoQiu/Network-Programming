#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30
// 显示错误信息
void error_handling(char *message);

int main(int argc, char const *argv[])
{
    int recv_sock;
    int str_len;
    char buf[BUF_SIZE];
    struct sockaddr_in adr;

    if (argc != 2)
    {
        printf("Usage : %s <PORT>\n", argv[0]);
        exit(1);
    }

    // 创建UDP套接字
    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    // 设置IP地址和端口号
    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(atoi(argv[1]));

    // 绑定端口号和IP地址
    if (bind(recv_sock, (struct sockaddr *)&adr, sizeof(adr)) == -1)
        error_handling("bind() error");

    while (1)
    {
        // 通过recvfrom函数接受数据。如果不需要知道传输数据的主机地址信息，可以项recvfrom函数的第5、6参数分别传入NULL, 0
        str_len = recvfrom(recv_sock, buf, BUF_SIZE - 1, 0, NULL, 0);
        if (str_len < 0)
            break;

        buf[str_len] = 0;
        fputs(buf, stdout);
    }

    close(recv_sock);
    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}