#include <stdio.h>
#include <pthread.h>

// 线程处理函数
void *thread_summation(void *arg);

// 共享的内存
int sum = 0;

int main(int argc, char *argv[])
{
    pthread_t id_t1, id_t2;
    int range1[] = {1, 5};
    int range2[] = {6, 10};

    // 创建两个线程
    pthread_create(&id_t1, NULL, thread_summation, (void *)range1);
    pthread_create(&id_t2, NULL, thread_summation, (void *)range2);

    pthread_join(id_t1, NULL);
    pthread_join(id_t2, NULL);

    printf("result: %d \n", sum);
    return 0;
}

// 线程处理函数
void *thread_summation(void *arg)
{
    int start = ((int *)arg)[0];
    int end = ((int *)arg)[1];
    while (start <= end)
    {
        sum += start;
        start++;
    }

    return NULL;
}
