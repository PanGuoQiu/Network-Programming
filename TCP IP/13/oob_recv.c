#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define BUF_SIZE 30

// 函数的前置声明
void error_handling(char *message);                 // 显示错误信息
void urg_handler(int signo);                        // 信号处理函数

int acpt_sock;
int recv_sock;

int main(int argc, char const *argv[])
{
    struct sockaddr_in recv_adr, serv_adr;
    int str_len, state;
    socklen_t serv_adr_sz;
    struct sigaction act;                           // 信号结构体
    char buf[BUF_SIZE];

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 设置信号属性
    act.sa_handler = urg_handler;                   // 函数地址
    sigemptyset(&act.sa_mask);                      // 清空内存
    act.sa_flags = 0;

    // 创建套接字
    acpt_sock = socket(PF_INET, SOCK_STREAM, 0);
    // 设置IP地址和端口号
    memset(&recv_adr, 0, sizeof(recv_adr));
    recv_adr.sin_family = AF_INET;
    recv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    recv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定IP地址和端口号
    if (bind(acpt_sock, (struct sockaddr *)&recv_adr, sizeof(recv_adr)) == -1)
        error_handling("bind() error");

    // 监听
    listen(acpt_sock, 5);

    // 接受连接请求
    serv_adr_sz = sizeof(serv_adr);
    recv_sock = accept(acpt_sock, (struct sockaddr *)&serv_adr, &serv_adr_sz);

    // 将文件描述符，recv_sock指向的套接字拥有者(F_SETOWN)改为把getpid函数返回值用做id的进程
    fcntl(recv_sock, F_SETOWN, getpid());
    // SIGURG是一个信号，当接受到MSG_OOB紧急消息时，系统产生SIGURG信号
    state = sigaction(SIGURG, &act, 0);

    while ((str_len = recv(recv_sock, buf, sizeof(buf), 0)) != 0)
    {
        if (str_len == -1)
            continue;

        buf[str_len] = 0;
        puts(buf);
    }

    close(recv_sock);
    close(acpt_sock);
    return 0;
}

// 信号处理函数
void urg_handler(int signo)
{
    int str_len;
    char buf[BUF_SIZE];

    str_len = recv(recv_sock, buf, sizeof(buf) - 1, MSG_OOB);   // 紧急处理函数
    buf[str_len] = 0;

    printf("Urgent message: %s \n", buf);
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}