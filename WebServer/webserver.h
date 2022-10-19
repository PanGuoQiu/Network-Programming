#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD			= 65536;			// 最大文件描述符
const int MAX_EVENT_NUMBER	= 10000;			// 最大事件数
const int TIMESLOT			= 5;				// 最小超时单位

// Web服务器
class WebServer
{
public:
	// 构造函数和析构函数
	WebServer();
	~WebServer();

	// 初始化服务器
	// 参数：端口号、登录名、密码、数据库名
	//		 日志写入方式、关闭连接、触发组合模式、数据库连接池数量
	//		 线程池内的线程数量、是否关闭日志、并发模型选择
	void init(int port, string user, string passWord, string databaseName,
			  int log_write, int opt_linger, int trigmode, int sql_num,
			  int thread_num, int close_log, int actor_model);

	void thread_pool();											// 线程池
	void sql_pool();											// 数据库连接池
	void log_write();											// 更改日志写入方式
	void trig_mode();											// 更改触发组合模式
	void eventListen();											// 创建事件监听描述符lfd
	void eventLoop();											// 当服务器非关闭状态，用于处理事件

	// 定时器的操作
	void timer(int connfd, struct sockaddr_in client_address);	// 定时器
	void adjust_timer(util_timer *timer);						// 适配定时器
	void deal_timer(util_timer *timer, int sockfd);				// 处理定时器

	// 处理事件函数
	bool dealclientdata();										// 处理用户数据
	bool dealwithsignal(bool &timer, bool &stop_server);		// 处理信号
	void dealwithread(int sockfd);								// 处理读事件
	void dealwithwrite(int sockfd);								// 处理写事件

public:
	// 自定义变量
	int m_port;									// 端口号
	char *m_root;								// 根目录
	int m_log_write;							// 日志写入方式
	int m_close_log;							// 日志开关
	int m_actormodel;							// 并发模式
	int m_pipefd[2];							// 进程通信模块
	int m_epollfd;								// 文件描述符epollfd

	http_conn *users;							// 用于接受http服务连接
	connection_pool *m_connPool;				// 数据库连接池
	string m_user;								// 登录数据库用户名
	string m_passWord;							// 登录数据库密码
	string m_databaseName;						// 使用的数据库名
	int m_sql_num;								// 数据库连接池数量

	threadpool<http_conn> *m_pool;				// 线程池模板处理http服务
	int m_thread_num;							// 线程池内线程的数量

	epoll_event events[MAX_EVENT_NUMBER];		// 监听事件队列的最大事件数

	int m_listenfd;								// 监听fd，申请一次
	int m_OPT_LINGER;							// 关闭连接，模式
	int m_TRIGMode;								// 触发模式ET+LE、LT+LT、LT+ET、ET+ET
	int m_LISTENTrigmode;						// 监听模式为ET+LT
	int m_CONNTrigmode;							// 连接模式为ET+LT

	client_data *users_timer;					// 客户端定时器
	Utils utils;								// 效用类
};

#endif // WEBSERVER_H

