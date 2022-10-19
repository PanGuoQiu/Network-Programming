#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

// 构造函数
connection_pool::connection_pool()
{
	m_CurConn = 0;									// 当前连接数
	m_FreeConn = 0;									// 当前空闲数
}

// 析构函数
connection_pool::~connection_pool()
{
	DestroyPool();
}

//单例模式： 获得连接池实例
connection_pool* connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

// 构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;							// 主机地址
	m_Port = Port;							// 端口
	m_User = User;							// 用户名
	m_PassWord = PassWord;					// 密码
	m_DatabaseName = DBName;				// 数据库名
	m_close_log = close_log;				// 日志开关

	// 创建连接池的所有数据库连接
	for (int i = 0; i < MaxConn; i++)
	{
		// 创建并初始化数据库
		MYSQL *con = NULL;
		con = mysql_init(con);
		if (con == NULL)
		{
			// 创建sql库失败，写入日志中
			LOG_ERROR("MySQL Error");
			exit(1);
		}

		// 连接数据库
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
		if (con == NULL)
		{
			// 在日志中，写入错误信息
			LOG_ERROR("MySQL Error");
			exit(1);
		}

		// 加入连接池
		connList.push_back(con);
		++m_FreeConn;								// 空闲连接数加1
	}

	reserve = sem(m_FreeConn);						// 初始化信号量为空闲连接数

	m_MaxConn = m_FreeConn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	// 如果连接池为空，则返回空
	if (0 == connList.size())
		return NULL;

	// 等待信号量不阻塞
	reserve.wait();

	lock.lock();

	// 获得列表的第一个连接，并弹栈
	con = connList.front();
	connList.pop_front();

	// 空闲连接减少、连接数增加
	--m_FreeConn;
	++m_CurConn;

	lock.unlock();

	return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	// 如果为空，返回错误
	if (NULL == con)
		return false;

	lock.lock();

	// 压栈，并修改空闲和连接数
	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	// 释放信号量
	reserve.post();
	
	return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool()
{
	lock.lock();

	// 如果存在数据库连接，则销毁
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}

		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

// 当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

// 连接池管理类：构造时自动连接、析构时自动释放
// 构造函数
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
	// 自动连接
	*SQL = connPool->GetConnection();

	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
	// 自动释放
	poolRAII->ReleaseConnection(conRAII);
}
