#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

// 显示错误信息
void error_handling(char *message);

int main(int argc, char const *argv[])
{
    int i;
    struct hostent *host;
    struct sockaddr_in addr;

    if (argc != 2)
    {
        printf("Usage : %s <IP>\n", argv[0]);
        exit(1);
    }

    // 设置IP地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    // 根据IP地址获取域名
    host = gethostbyaddr((char *)&addr.sin_addr, 4, AF_INET);
    if (!host)
        error_handling("gethost.... error");

    // 官方域名
    printf("Official name: %s \n", host->h_name);
    // 一个IP对应多个域名
    for (i = 0; host->h_aliases[i]; i++)
        printf("Aliases %d: %s \n", i + 1, host->h_aliases[i]);
    // IP类型
    printf("Address type: %s \n", (host->h_addrtype == AF_INET) ? "AF_INET" : "AF_INET6");
    // 一个域名对应多个IP
    for (i = 0; host->h_addr_list[i]; i++)
        printf("IP addr %d: %s \n", i + 1, inet_ntoa(*(struct in_addr *)host->h_addr_list[i]));

    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}