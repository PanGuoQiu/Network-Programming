#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 信号量类
class sem
{
public:
	// 构造函数
	sem()
	{
		// 初始化信号量，不成功，则抛出异常
		if (sem_init(&m_sem, 0, 0) != 0)
		{
			throw std::exception();
		}
	}

	// 重载构造函数
	sem(int num)
	{
		// 初始化信号量的初始值为num
		if (sem_init(&m_sem, 0, num) != 0)
		{
			throw std::exception();
		}
	}

	// 析构函数
	~sem()
	{
		// 销毁信号量
		sem_destroy(&m_sem);
	}

	// 信号量等待，阻塞
	bool wait()
	{
		// 判断是否阻塞
		return sem_wait(&m_sem) == 0;
	}

	// 是否释放信号量
	bool post()
	{
		return sem_post(&m_sem) == 0;
	}

private:
	sem_t m_sem;					// 信号量
};

// 互斥锁
class locker
{
public:
	// 构造函数
	locker()
	{
		// 判断互斥锁是否创建成功，失败则抛出异常
		if (pthread_mutex_init(&m_mutex, NULL) != 0)
		{
			throw std::exception();
		}
	}

	// 析构函数
	~locker()
	{
		// 销毁互斥锁
		pthread_mutex_destroy(&m_mutex);
	}

	// 是否加锁
	bool lock()
	{
		return pthread_mutex_lock(&m_mutex) == 0;
	}

	// 是否解锁
	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex) == 0;
	}

	// 获得互斥锁的指针
	pthread_mutex_t *get()
	{
		return &m_mutex;
	}

private:
	pthread_mutex_t m_mutex;					// 互斥锁
};

// 条件变量
class cond
{
public:
	// 构造函数
	cond()
	{
		// 初始化一个缺省的条件变量，失败则抛出异常
		if (pthread_cond_init(&m_cond, NULL) != 0)
		{
			// pthread_mutex_destroy(&m_mutex);
			throw std::exception();
		}
	}

	// 析构函数
	~cond()
	{
		// 释放条件变量
		pthread_cond_destroy(&m_cond);
	}

	// 是否阻塞在条件变量上
	bool wait(pthread_mutex_t* m_mutex)
	{
		int ret = 0;
		//pthread_mutex_lock(&m_mutex);
		ret = pthread_cond_wait(&m_cond, m_mutex);
		//pthread_mutex_unlock(&m_mutex);
		return ret == 0;
	}

	// 是否阻塞到指定的时间
	bool timewait(pthread_mutex_t* m_mutex, struct timespec t)
	{
		int ret = 0;
		//pthread_mutex_lock(&m_mutex);
		ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
		//pthread_mutex_unlock(&m_mutex);
		return ret == 0;
	}

	// 是否解除条件变量上的阻塞
	bool signal()
	{
		return pthread_cond_signal(&m_cond) == 0;
	}

	// 是否释放阻塞的所有线程
	bool broadcast()
	{
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	// static pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;							// 条件变量
};

#endif // LOCKER_H

