#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// 线程处理函数
void *thread_main(void *arg);

int main(int argc, char *argv[])
{
    pthread_t t_id;
    int thread_param = 5;
    void *thr_ret;

    // 请求创建一个线程，从thread_main调用开始，在单独的执行流中运行。同时传递参数
    if (pthread_create(&t_id, NULL, thread_main, (void *)&thread_param) != 0)
    {
        puts("pthread_create() error");
        return -1;
    }

    // main函数将等待ID保存在t_id变量中的线程终止，thr_ret保存线程的返回值
    if (pthread_join(t_id, &thr_ret) != 0)
    {
        puts("pthread_join() error");
        return -1;
    }

    printf("Thread return message: %s \n", (char *)thr_ret);
    free(thr_ret);
    return 0;
}

// 线程处理函数:传入的参数是pthread_create的第四个参数，返回值保存在main函数的thr_ret中
void *thread_main(void *arg)
{
    int i;
    int cnt = *((int *)arg);
    char *msg = (char *)malloc(sizeof(char) * 50);
    strcpy(msg, "Hello,I'am thread~ \n");
    for (i = 0; i < cnt; i++)
    {
        sleep(1);
        puts("running thread");
    }

    // 返回值是thread_main函数中内部动态分配的内存空间地址值
    return (void *)msg;
}
