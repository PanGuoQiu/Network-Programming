#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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
    struct ip_mreq join_adr;

    if (argc != 3)
    {
        printf("Usage : %s <GroupIP> <PORT>\n", argv[0]);
        exit(1);
    }

    // 创建UDP套接字
    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(atoi(argv[2]));

    // 绑定IP地址和端口号
    if (bind(recv_sock, (struct sockaddr *)&adr, sizeof(adr)) == -1)
        error_handling("bind() error");

    // 初始化结构体
    join_adr.imr_multiaddr.s_addr = inet_addr(argv[1]);     // 多播组地址
    join_adr.imr_interface.s_addr = htonl(INADDR_ANY);      // 待加入的IP地址
    // 利用套接字选项IP_ADD_MEMBERSHIP加入多播组，完成了接受指定的多播组数据的所有准备
    setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&join_adr, sizeof(join_adr));

    while (1)
    {
        // 通过recvfrom函数接受多播数据。如果不需要知道传输数据的主机地址信息，可以项recvfrom函数的第5、6参数分别传入NULL、0
        str_len = recvfrom(recv_sock, buf, BUF_SIZE - 1, 0, NULL, 0);
        if (str_len < 0)
            break;

        buf[BUF_SIZE] = 0;
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