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
    int sock;
    char message[BUF_SIZE];
    int str_len;
    socklen_t adr_sz;

    struct sockaddr_in serv_adr, from_adr;
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // 创建UDP套接字
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 设置IP地址和端口
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    while (1)
    {
        fputs("Insert message(q to quit): ", stdout);
        fgets(message, sizeof(message), stdin);
        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        // 向服务器传输数据，会自动给自己分配IP地址和端口号
        sendto(sock, message, strlen(message), 0, (struct sockaddr *)&serv_adr, sizeof(serv_adr));

        adr_sz = sizeof(from_adr);
        str_len = recvfrom(sock, message, BUF_SIZE, 0, (struct sockaddr *)&from_adr, &adr_sz);

        message[str_len] = 0;
        printf("Message form server: %s", message);
    }

    close(sock);
    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
