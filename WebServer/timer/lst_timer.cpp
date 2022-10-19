#include "lst_timer.h"
#include "../http/http_conn.h"

// 排序定时器列表类
// 构造函数
sort_timer_lst::sort_timer_lst()
{
	head = NULL;
	tail = NULL;
}

// 析构函数
sort_timer_lst::~sort_timer_lst()
{
	// 遍历删除所有元素
	util_timer *tmp = head;
	while (tmp)
	{
		head = tmp->next;
		delete tmp;
		tmp = head;
	}
}

// 增加定时器
void sort_timer_lst::add_timer(util_timer *timer)
{
	if (!timer)
		return;

	if (!head)
	{
		head = tail = timer;
		return;
	}

	// 插入的定时器失效时间小于第一个定时器的，头插法
	if (timer->expire < head->expire)
	{
		timer->next = head;
		head->prev = timer;
		head = timer;
		return;
	}

	// 在头部增加一个定时器
	add_timer(timer, head);
}

// 整理定时器
void sort_timer_lst::adjust_timer(util_timer *timer)
{
	if (!timer)
		return;

	// 根据失效时间排序
	util_timer *tmp = timer->next;
	if (!tmp || (timer->expire < tmp->expire))
	{
		return;
	}

	// 判断是否为头部
	if (timer == head)
	{
		// 头部，则在后面插入
		head = head->next;
		head->prev = NULL;
		timer->next = NULL;
		add_timer(timer, head);
	}
	else
	{
		// 不是头部，则在timer下一个的后面插入
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);
	}
}

// 删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
	if (!timer)
		return;

	// 只有一个定时器
	if ((timer == head) && (timer == tail))
	{
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}

	// 删除头部定时器
	if (timer == head)
	{
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}

	// 删除尾部定时器
	if (timer == tail)
	{
		tail = tail->prev;
		tail->next = NULL;
		delete tail;
		return;
	}

	// 删除中间的定时器
	timer->prev->next = timer->next;				// timer的前面一个跳过timer指向timer的后面一个
	timer->next->prev = timer->prev;				// timer的后面一个跳过timer指向timer的前面一个
	delete timer;									// 删除timer
}

// 每经过一个最下时间片段，调用一次处理函数
void sort_timer_lst::tick()
{
	if (!head)
		return;

	time_t cur = time(NULL);
	util_timer *tmp = head;
	while (tmp)
	{
		// 判断当前的时间是否小于失效时间
		if (cur < tmp->expire)
		{
			break;
		}

		// 处理定时器已过期的事件，并销毁定时器
		tmp->cb_func(tmp->user_data);
		head = tmp->next;
		if (head)
		{
			head->prev = NULL;
		}

		delete tmp;
		tmp = head;
	}
}

// 在指定定时器后面插入一个定时器
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
	util_timer *prev = lst_head;
	util_timer *tmp = prev->next;
	while (tmp)
	{
		// 根据expire过期时间插入，升序排列
		if (timer->expire < tmp->expire)
		{
			// 在prev后面插入定时器
			prev->next = timer;
			timer->next = tmp;
			tmp->prev = timer;
			timer->prev = prev;
			break;
		}

		// 遍历寻找下一个
		prev = tmp;
		tmp = tmp->next;
	}

	// 判断prev后面是否有定时器，如果没有,就是在尾部，则直接插入
	if (!tmp)
	{
		prev->next = timer;
		timer->prev = prev;
		timer->next = NULL;
		tail = timer;
	}
}


// 效用类
// 初始化
void Utils::init(int timeslot)
{
	m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
	// fcntl函数改变文件的性质
	int old_option = fcntl(fd, F_GETFL);			// 取得文件描述符
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);					// 设置文件描述符

	return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode)
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	else
		event.events = EPOLLIN | EPOLLRDHUP;

	if (one_shot)
		event.events |= EPOLLONESHOT;

	// 操作内核事件表监控的文件描述符上的事件：注册一个事件
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	// 对文件描述符设置非阻塞
	setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig)
{
	// 为保证函数的可重入性，保留原来的errno
	int save_errno = errno;
	int msg = sig;
	send(u_pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;

	if (restart)
		sa.sa_flags |= SA_RESTART;

	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
	// 每过一个最小时间片段调用一次回调函数
	m_timer_lst.tick();
	alarm(m_TIMESLOT);
}

// 显示错误
void Utils::show_error(int connfd, const char *info)
{
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// 回调函数
class Utils;
void cb_func(client_data *user_data)
{
	// 删除一个fd
	epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);

	// http连接用户减1
	http_conn::m_user_count--;
}

