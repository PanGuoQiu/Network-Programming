#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256

// 函数的前置声明
void *handle_clnt(void *arg);                       // 客户端句柄处理函数
void send_msg(char *msg, int len);                  // 发送消息
void error_handling(char *msg);                     // 显示错误信息

int clnt_cnt = 0;                                   // 客户端的数量
int clnt_socks[MAX_CLNT];                           // 存储客户端套接字的数组
pthread_mutex_t mutx;                               // 互斥量

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 创建互斥锁
    pthread_mutex_init(&mutx, NULL);

    // 创建套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    // 设置IP地址和端口号
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定IP地址和端口号
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 监听
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1)
    {
        // 接受请求连接
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);                  // 上锁
        clnt_socks[clnt_cnt++] = clnt_sock;         // 写入新连接
        pthread_mutex_unlock(&mutx);                // 解锁

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);       // 创建线程为新客户端服务，并且把clnt_sock作为参数传递
        pthread_detach(t_id);                                               // 引导线程销毁，不阻塞
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr)); // 客户端连接的IP地址
    }

    close(serv_sock);
    return 0;
}

// 处理客户端的连接或者请求
void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);                  // 获得线程函数的参数
    int str_len = 0, i;
    char msg[BUF_SIZE];

    // 接收数据，并发送出去
    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0)
        send_msg(msg, str_len);

    // 接收到消息为0，代表当前客户端已经断开连接
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)                  // 删除没有连接的客户端
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);

    close(clnt_sock);
    return NULL;
}

// 项连接的所有客户端发送消息
void send_msg(char *msg, int len)
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], msg, len);
    pthread_mutex_unlock(&mutx);
}

// 显示错误信息
void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}