/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

// 模板队列块
template <class T>
class block_queue
{
public:
	// 构造函数
	block_queue(int max_size = 1000)
	{
		// 队列块小于0则退出
		if (max_size <= 0)
		{
			exit(-1);
		}

		// 初始化
		m_max_size = max_size;							// 队列最大值
		m_array = new T[max_size];						// 分配队列内存
		m_size = 0;										// 当前队列大小
		m_front = -1;									// 队首下标
		m_back = -1;									// 队尾下标
	}

	// 清除队列
	void clear()
	{
		m_mutex.lock();

		m_size = 0;
		m_front = -1;
		m_back = -1;

		m_mutex.unlock();
	}

	// 析构函数
	~block_queue()
	{
		m_mutex.lock();

		// 释放队列内存
		if (m_array != NULL)
			delete[] m_array;

		m_mutex.unlock();
	}

	// 判断队列是否存满
	bool full()
	{
		m_mutex.lock();

		if (m_size >= m_max_size)
		{
			m_mutex.unlock();
			return true;
		}

		m_mutex.unlock();
		return false;
	}

	// 判断队列是否为空
	bool empty()
	{
		m_mutex.lock();
		if (0 == m_size)
		{
			m_mutex.unlock();
			return true;
		}

		m_mutex.unlock();
		return false;
	}

	// 返回队首元素是否成功
	bool front(T &value)
	{
		m_mutex.lock();
		if (0 == m_size)
		{
			m_mutex.unlock();
			return false;
		}

		value = m_array[m_front];						// 获得队首元素

		m_mutex.unlock();
		return true;
	}

	// 返回队尾元素，并判断是否成功
	bool back(T &value)
	{
		m_mutex.lock();
		if (0 == m_size)
		{
			m_mutex.unlock();
			return false;
		}

		value = m_array[back];							// 获得队尾元素

		m_mutex.unlock();
		return true;
	}

	// 获得当前队列的大小
	int size()
	{
		int tmp = 0;

		m_mutex.lock();
		tmp = m_size;
		m_mutex.unlock();

		return tmp;
	}

	// 获得队列的最大值
	int max_size()
	{
		int tmp = 0;

		m_mutex.lock();
		tmp = m_max_size;
		m_mutex.unlock();

		return tmp;
	}

	// 往队列添加元素，需要将所有使用队列的线程唤醒
	// 当有元素push进队列，相当于生成者生成一个元素
	// 若当前没有线程等待条件变量，则唤醒无意义
	bool push(const T &item)
	{
		m_mutex.lock();
		// 存满，则失败
		if (m_size >= m_max_size)
		{
			m_cond.broadcast();
			m_mutex.unlock();
			return false;
		}

		// 队尾下标下移一位，并存入数据
		m_back = (m_back + 1) % m_max_size;
		m_array[m_back] = item;

		m_size++;										// 当前队列大小加1

		m_cond.broadcast();
		m_mutex.unlock();
		return true;
	}

	// pop时，如果当前队列没有元素，将会等待条件变量
	bool pop(T &item)
	{
		m_mutex.lock();
		// 队列没有元素
		while (m_size <= 0)
		{
			// 是否在阻塞在条件变量：互斥锁上
			if (!m_cond.wait(m_mutex.get()))
			{
				m_mutex.unlock();
				return false;
			}
		}

		// 队首加1，并获得队首元素
		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];

		m_size--;										// 队列大小减1

		m_mutex.unlock();
		return true;
	}

	// 增加了超时处理
	bool pop(T &item, int ms_timeout)
	{
		struct timespec t = { 0, 0 };
		struct timeval now = { 0, 0 };
		gettimeofday(&now, NULL);
		m_mutex.lock();
		if (m_size <= 0)
		{
			t.tv_sec = now.tv_sec + ms_timeout / 1000;	// 秒为单位，超时的时间
			t.tv_nsec = (ms_timeout % 1000) * 1000;		// 微妙为单位，转换为秒
			// 是否阻塞到指定的时间：互斥锁
			if (!m_cond.timewait(m_mutex.get(), t))
			{
				m_mutex.unlock();
				return false;
			}
		}

		if (m_size <= 0)
		{
			m_mutex.unlock();
			return false;
		}

		// 获得队首元素
		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];

		m_size--;										// 队列大小减1

		m_mutex.unlock();
		return true;
	}

private:
	locker m_mutex;										// 互斥锁
	cond   m_cond;										// 条件变量

	T *m_array;											// 队列指针
	int m_size;											// 当前队列大小
	int m_max_size;										// 队列的最大长度
	int m_front;										// 队首下标
	int m_back;											// 队尾下标
};

#endif // BLOCK_QUEUE_H

