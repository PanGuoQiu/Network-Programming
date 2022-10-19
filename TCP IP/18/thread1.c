#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

// 线程处理函数
void *thread_main(void *arg);

int main(int argc, char *argv[])
{
    pthread_t t_id;
    int thread_param = 5;
    // 请求创建一个线程，从thread_main调用开始，在单独的执行流中运行。同时传递参数
    if (pthread_create(&t_id, NULL, thread_main, (void*)&thread_param) != 0)
    {
        puts("pthread_create() error");
        return -1;
    }

    sleep(10);                                      // 延迟进程终止时间
    puts("end of main");
    return 0;
}

// 线程执行函数：传入的参数时pthread_create的第四个
void *thread_main(void *arg)
{
    int i;
    int cnt = *((int *)arg);
    for (i = 0; i < cnt; i++)
    {
        sleep(1);
        puts("running thread");
    }

    return NULL;
}