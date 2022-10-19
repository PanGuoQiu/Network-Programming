#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREAD 100

// 线程处理函数
void *thread_inc(void *arg);                        // 增加
void *thread_des(void *arg);                        // 减少

long long num = 0;

int main(int argc, char *argv[])
{
    pthread_t thread_id[NUM_THREAD];
    int i;

    printf("sizeof long long: %d \n", sizeof(long long));
    for (i = 0; i < NUM_THREAD; i++)
    {
        if (i % 2)
            pthread_create(&(thread_id[i]), NULL, thread_inc, NULL);
        else
            pthread_create(&(thread_id[i]), NULL, thread_des, NULL);
    }

    for (i = 0; i < NUM_THREAD; i++)
        pthread_join(thread_id[i], NULL);

    printf("result: %lld \n", num);
    return 0;
}

// 自增
void *thread_inc(void *arg)
{
    int i;
    for (i = 0; i < 50000000; i++)
        num += 1;

    return NULL;
}

// 自减
void *thread_des(void *arg)
{
    int i;
    for (i = 0; i < 50000000; i++)
        num -= 1;

    return NULL;
}