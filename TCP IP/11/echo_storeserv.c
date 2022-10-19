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
void error_handling(char *message);                         // 显示错误信息
void read_childproc(int sig);                               // 信号处理函数

int main(int argc, char const *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int fds[2];

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
    act.sa_handler = read_childproc;                        // 防止僵尸进程
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    state = sigaction(SIGCHLD, &act, 0);                    // 注册信号处理器，把成功的返回值给state

    // 创建套接字并设置IP地址和端口号
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定：分配IP地址和端口号
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 监听：进入等待连接请求状态
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 创建管道
    pipe(fds);

    // 创建子进程1
    pid = fork();
    if (pid == 0)
    {
        // 读取信息，并写入文本中
        FILE *fp = fopen("echomsg.txt", "wt");
        char msgbuf[BUF_SIZE];
        int i, len;
        for (i = 0; i < 10; i++)
        {
            len = read(fds[0], msgbuf, BUF_SIZE);
            fwrite((void*)msgbuf, 1, len, fp);
        }

        fclose(fp);
        return 0;
    }

    while (1)
    {
        // 接受连接，并生成一个客户端文件描述符
        adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
        if (clnt_sock == -1)
            error_handling("accept() error");
        else
            puts("new client connected ...");

        // 创建子进程2：父进程处理接受连接请求；子进程2提供具体服务
        pid = fork();
        if (pid == 0)
        {
            close(serv_sock);                               // 关闭服务器套接字，因为从父进程传递到了子进程
            while ((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0)
            {
                write(clnt_sock, buf, str_len);             // 输出到客户端的信息
                write(fds[1], buf, str_len);                // 输出到管道的信息
            }

            close(clnt_sock);
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
