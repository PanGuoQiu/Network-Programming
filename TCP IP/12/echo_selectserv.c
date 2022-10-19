#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 100

// 显示错误信息
void error_handling(char *message);

int main(int argc, char const *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    struct timeval timeout;
    fd_set reads, cpy_reads;

    socklen_t adr_sz;
    int fd_max, str_len, fd_num, i;
    char buf[BUF_SIZE];

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 创建套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // 设置IP地址和端口号
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 套接字绑定IP地址和端口号
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 监听端口号
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 设置文件描述符
    FD_ZERO(&reads);                                            // 初始化变量
    FD_SET(serv_sock, &reads);                                  // 注册服务端套接字
    fd_max = serv_sock;

    while (1)
    {
        cpy_reads = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;

        // 开始监听，每次重新监听
        if ((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
            break;
        if (fd_num == 0)
            continue;

        for (i = 0; i < fd_max + 1; i++)
        {
            // 查找发生变化的套接字文件描述符
            if (FD_ISSET(i, &cpy_reads))
            {
                // 如果是服务端套接字时，受理连接请求
                if (i == serv_sock)
                {
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);

                    // 注册一个clnt_sock
                    FD_SET(clnt_sock, &reads);
                    if (fd_max < clnt_sock)
                        fd_max = clnt_sock;

                    printf("Connected client: %d \n", clnt_sock);
                }
                else                                            // 不是服务端套接字时
                {
                    str_len = read(i, buf, BUF_SIZE);           // i指的是当前发起请求的客户端
                    if (str_len == 0)
                    {
                        FD_CLR(i, &reads);                      // 清除i的文件描述符信息
                        close(i);
                        printf("closed client: %d \n", i);
                    }
                    else
                    {
                        // 向客户端输出数据
                        write(i, buf, str_len);
                    }
                }
            }
        }
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
