#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

// 显示错误信息
void error_handling(char *message);

int main(int argc, char const *argv[])
{
    char *addr = "127.232.124.78";
    struct sockaddr_in addr_inet;

    if (!inet_aton(addr, &addr_inet.sin_addr))
        error_handling("Conversion error");
    else
        printf("Network ordered integer addr: %#x \n", addr_inet.sin_addr.s_addr);

    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}