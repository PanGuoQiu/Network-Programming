#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

// 数据库连接池类
class connection_pool
{
public:
	MYSQL *GetConnection();						// 获取数据库连接
	bool  ReleaseConnection(MYSQL *conn);		// 释放连接
	int   GetFreeConn();						// 获取连接
	void  DestroyPool();						// 销毁所有连接

	// 单例模式
	static connection_pool *GetInstance();

	// 初始化 /主机地址、用户、密码、数据库名、端口、最大连接数、日志开关
	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
	// 构造函数和析构函数
	connection_pool();
	~connection_pool();

	int m_MaxConn;								// 最大连接数
	int m_CurConn;								// 当前已使用的连接数
	int m_FreeConn;								// 当前空闲的连接数

	locker lock;								// 互斥锁
	list<MYSQL *> connList;						// 连接池
	sem reserve;								// 信号量

public:
	string m_url;								// 主机地址
	string m_Port;								// 数据库端口号
	string m_User;								// 登录数据库用户名
	string m_PassWord;							// 登录数据库密码
	string m_DatabaseName;						// 使用数据库名
	int m_close_log;							// 日志开关
};

/*
Resource Acquisition Is Initialization, c++的一种资源管理机制
将资源调用封装为类的形式，在析构函数中释放资源，当对象生命结束时，系统会自动调用析构函数
通过connectionRAII类改变外部MYSQL指针使其指向连接池的连接，析构函数归还连接入池
*/
class connectionRAII
{
public:
	// 构造函数和析构函数
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();

private:
	MYSQL *conRAII;								// 数据库指针
	connection_pool *poolRAII;					// 数据库连接池类
};

#endif // _CONNECTION_POOL_

