#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

// 实用定时器
class util_timer;

// 客户端数据
struct client_data
{
	sockaddr_in address;					// 套接字地址
	int sockfd;								// 套接字fd
	util_timer *timer;						// 定时器
};

// 实用定时器
class util_timer
{
public:
	// 构造函数
	util_timer() : prev(NULL), next(NULL) {}

public:
	time_t expire;									// 定时器过期失效

	void (* cb_func)(client_data *);				// 回调函数

	client_data *user_data;							// 客户端数据
	util_timer *prev;								// 前一个定时器
	util_timer *next;								// 下一个定时器
};

// 排序定时器列表
class sort_timer_lst
{
public:
	// 构造函数和析构函数
	sort_timer_lst();
	~sort_timer_lst();

	void add_timer(util_timer *timer);				// 增加定时器
	void adjust_timer(util_timer *timer);			// 适配定时器
	void del_timer(util_timer *timer);				// 删除定时器
	void tick();									// 最小时间片段

private:
	// 增加定时器到列表头部
	void add_timer(util_timer *timer, util_timer *lst_head);

	util_timer *head;								// 列表头部
	util_timer *tail;								// 列表尾部
};

// 效用类
class Utils
{
public:
	// 构造函数和析构函数
	Utils() {}
	~Utils() {}

	// 初始化
	void init(int timeslot);

	// 对文件描述符设置非阻塞
	int setnonblocking(int fd);

	// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
	void addfd(int epellfd, int fd, bool one_shot, int TRIGMode);

	// 信号处理函数
	static void sig_handler(int sig);

	// 设置信号函数
	void addsig(int sig, void(handler)(int), bool restart = true);

	// 定时处理任务，重新定时以不断触发SIGALRM信号
	void timer_handler();

	// 显示错误
	void show_error(int connfd, const char *info);

public:
	static int *u_pipefd;							// 管道中的fd，使用指针作为管道
	sort_timer_lst m_timer_lst;						// 排序定时器列表
	static int u_epollfd;							// epoll监听的fd
	int m_TIMESLOT;									// 排序的时间
};

// 回调的函数
void cb_func(client_data *user_data);

#endif // LST_TIMER

