#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

// 线程池模板类
template <typename T>
class threadpool
{
public:
	// thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量
	threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
	~threadpool();

	bool append(T *request, int state);
	bool append_p(T *request);

private:
	// 工作线程运行的函数，它不断从工作队列中取出任务并执行之
	static void *worker(void *arg);
	void run();

private:
	int					m_thread_number;		// 线程池中的线程数
	int					m_max_requests;			// 请求队列中允许的最大请求数
	pthread_t*			m_threads;				// 描述线程池的数组，其大小为m_thread_number
	std::list<T *>		m_workqueue;			// 请求队列
	locker				m_queuelocker;			// 保护请求队列的互斥锁
	sem					m_queuestat;			// 是否有任务需要处理
	connection_pool*	m_connPool;				// 数据库连接池
	int					m_actor_model;			// 模型切换(Reactor/Proactor)
};

// 构造函数
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
	// 判断线程数和队列中的请求数是否异常
	if (thread_number <= 0 || max_requests <= 0)
		throw std::exception();

	// 分配线程池内存
	m_threads = new pthread_t[m_thread_number];
	if (!m_threads)
		throw std::exception();

	// 创建线程池中的每个线程
	for (int i = 0; i < thread_number; ++i)
	{
		// 判断是否创建线程成功，worker为线程运行函数的起始地址
		if (pthread_create(m_threads + i, NULL, worker, this) != 0)
		{
			// 失败，销毁内存并抛出异常
			delete[] m_threads;
			throw std::exception();
		}

		// 判断线程分离是否成功
		// 主要是将线程属性更改为unjoinable，便于资源的释放
		if (pthread_detach(m_threads[i]))
		{
			// 失败，则释放内存并抛出异常
			delete[] m_threads;
			throw std::exception();
		}
	}
}

// 析构函数
template <typename T>
threadpool<T>::~threadpool()
{
	// 释放线程池内存
	delete[] m_threads;
}

// 在列表尾压入请求，并设置请求的状态
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
	m_queuelocker.lock();

	// 判断请求队列是否大于最大队列值
	if (m_workqueue.size() >= m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}

	// 设置请求的状态并压入队列中
	request->m_state = state;
	m_workqueue.push_back(request);

	m_queuelocker.unlock();

	m_queuestat.post();								// 是否有任务需要处理，释放信号量
	return true;
}

// proactor模式下的请求入队
template <typename T>
bool threadpool<T>::append_p(T *request)
{
	m_queuelocker.lock();

	// 判断请求队列是否大于队列最大值
	if (m_workqueue.size() >= m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}

	// 在列表尾部压入请求
	m_workqueue.push_back(request);

	m_queuelocker.unlock();							// 释放一个信号量

	m_queuestat.post();
	return true;
}

// 工作线程：pthread_create时就调用了它
template <typename T>
void* threadpool<T>::worker(void *arg)
{
	// 调用时*arg是this
	// 所以该操作其实是获取threadpool对象地址
	threadpool *pool = (threadpool *)arg;
	// 线程池中每一个线程创建是都会调用run()，睡眠在队列中
	pool->run();
	
	return pool;
}

// 线程池中的所有线程都睡眠，等待请求队列中新增任务
template <typename T>
void threadpool<T>::run()
{
	while (true)
	{
		m_queuestat.wait();
		m_queuelocker.lock();

		// 判断请求队列是否为空
		if (m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}

		// 获得列表的第一个请求，并弹出
		T *request = m_workqueue.front();
		m_workqueue.pop_front();

		m_queuelocker.unlock();

		// 如果不存在，则跳过这次循环
		if (!request)
			continue;

		// 处理模式
		if (1 == m_actor_model)
		{
			// 判断请求状态
			if (0 == request->m_state)
			{
				// 判断请求是否读取一次
				if (request->read_once())
				{
					request->improv = 1;
					connectionRAII mysqlcon(&request->mysql, m_connPool);
					request->process();
				}
				else
				{
					request->improv = 1;
					request->timer_flag = 1;
				}
			}
			else
			{
				if (request->write())
				{
					request->improv = 1;
				}
				else
				{
					request->improv = 1;
					request->timer_flag = 1;
				}
			}
		}
		else
		{
			connectionRAII mysqlcon(&request->mysql, m_connPool);
			request->process();
		}
	}
}

#endif // THREADPOOL_H

