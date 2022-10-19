#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30

// 函数声明
void error_handling(char *message);                 // 显示错误信息
void read_childproc(int sig);                       // 信号处理函数

int main(int argc, char const *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;

    pid_t pid;
    struct sigaction act;
    socklen_t adr_sz;
    int str_len, state;
    char buf[BUF_SIZE];

    if (argc != 2)
    {
        printf("Usgae : %s <port>\n", argv[0]);
        exit(1);
    }

    // 设置信号
    act.sa_handler = read_childproc;                            // 防止僵尸进程
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    state = sigaction(SIGCHLD, &act, 0);                        // 注册信号处理器，把成功的返回值给state

    // 设置IP地址和端口号
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);                // 创建服务端套接字
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定：分配IP地址和端口号
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 监听：等待连接请求状态
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1)
    {
        // 接受连接请求
        adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
        if (clnt_sock == -1)
            continue;
        else
            puts("new client connected...");

        // 创建子进程
        pid = fork();                                           // 此时，父子进程分别带有一个套接字
        if (pid == -1)
        {
            close(clnt_sock);
            continue;
        }

        // 子进程运行区域，此部分向客户端提供回声服务
        if (pid == 0)
        {
            close(serv_sock);                                   // 关闭服务器套接字，因为从父进程传递到了子进程

            // 提供回声服务
            while ((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0)
                write(clnt_sock, buf, str_len);

            close(clnt_sock);                                   // 关闭客户端套接字，因为子进程完成了回声服务
            puts("client disconnected ...");
            return 0;
        }
        else
            close(clnt_sock);   // 通过accept函数创建的套接字文件描述符已经复制给子进程，因为服务器端要销毁自己拥有的
    }

    close(serv_sock);
    return 0;
}

// 显示错误信息
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 信号处理函数
void read_childproc(int sig)
{
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    printf("removed proc id: %d \n", pid);
}
