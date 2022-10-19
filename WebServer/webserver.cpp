#include "webserver.h"

/*
// 构造函数
WebServer::WebServer()
{
	// 分配http类对象内存
	users = new http_conn[MAX_FD];

	// root文件路径
	char server_path[200];
//	getcwd(server_path, 200);				// 将工作目录的绝对路径赋值到path锁指向的空间中
	getcwd(server_path, 200);
	char root[6] = "/root";
	m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
	strcpy(m_root, server_path);
	strcat(m_root, root);

	// 定时器
	users_timer = new client_data[MAX_FD];	//客户端数据定时器
}*/
WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

// 析构函数
WebServer::~WebServer()
{
	// 释放资源
	close(m_epollfd);						// epollfd文件描述符
	close(m_listenfd);						// 监听文件描述符fd
	close(m_pipefd[1]);						// 进程通信模块1
	close(m_pipefd[0]);						// 进程通信模块0
	delete[] users;							// 释放http连接的用户
	delete[] users_timer;					// 释放客户端数据定时器
	delete m_pool;							// 线程池

}

// 初始化
void WebServer::init(int port, string user, string passWord, string databaseName, int log_write,
	int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
	m_port			= port;					// 端口号
	m_user			= user;					// 用户名
	m_passWord		= passWord;				// 密码
	m_databaseName	= databaseName;			// 数据库名
	m_sql_num		= sql_num;				// 数据库连接池数量
	m_thread_num	= thread_num;			// 线程池内线程的数量
	m_log_write		= log_write;			// 日志的写入方式
	m_OPT_LINGER	= opt_linger;			// 优雅关闭连接
	m_TRIGMode		= trigmode;				// 组合触发模式
	m_close_log		= close_log;			// 日志开关
	m_actormodel	= actor_model;			// 并发模型
}

// 更改触发组合模式
void WebServer::trig_mode()
{
	// LT + LT
	if (0 == m_TRIGMode)
	{
		m_LISTENTrigmode = 0;				// 监听模式为LT
		m_CONNTrigmode = 0;					// 连接模式为LT
	}
	// LT + ET
	else if (1 == m_TRIGMode)
	{
		m_LISTENTrigmode = 0;
		m_CONNTrigmode = 1;
	}
	// ET + LT
	else if (2 == m_TRIGMode)
	{
		m_LISTENTrigmode = 1;
		m_CONNTrigmode = 0;
	}
	// ET + ET
	else if (3 == m_TRIGMode)
	{
		m_LISTENTrigmode = 1;
		m_CONNTrigmode = 1;
	}
}

// 日志的写入方式
void WebServer::log_write()
{
	if (0 == m_close_log)
	{
		// 初始化日志
		if (1 == m_log_write)
			Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
		else
			Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
	}
}

// 数据库连接池
void WebServer::sql_pool()
{
	// 初始化数据库连接池
	m_connPool = connection_pool::GetInstance();
	m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

	// 初始化数据库读取表
	users->initmysql_result(m_connPool);
}

// 线程池
void WebServer::thread_pool()
{
	// 线程池
	m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

// 监听事件源：信号和其他文件描述符事件
// 完成了设置lfd与统一事件源，并且创建了带有一个节点的epoll树，同时完成了超时设定
void WebServer::eventListen()
{
	// 创建监听套接字
	m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(m_listenfd >= 0);

	// TCP连接断开的时候调用closesocket函数，有优雅的断开和强制断开两种方式
	// 优雅关闭连接
	if (0 == m_OPT_LINGER)
	{
		// 底层会将未发送完的数据发送完后再释放资源，也就是优雅的退出
		struct linger tmp = { 0, 1 };
		setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
	}
	else if (1 == m_OPT_LINGER)
	{
		// 这种方式下，在调用closesocket的时候不会立刻返回，内核会延迟一段时间，这个时间就由l_linger得值来决定。
		// 如果超时时间到达之前，发送完未发送的数据(包括FIN包)并得到一端的确认，closesocket会返回正确,socket描述符优雅退出
		// 否则，closesocket会直接返回，错误值，未发送数据丢失，socket描述符被强制性退出
		// 需要注意的是，如果socket描述符被设置为非堵塞型，则closesocket会直接返回值。
		struct linger tmp = { 1, 1 };
		setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
	}

	int ret = 0;
	struct sockaddr_in address;
	// bzero()会将内存块(字符串)的前n个字节清零
	// s为内存(字符串)指针，n为需要清零的字节数
	// 在网络编程中经常用到
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);	// 任意的本地ip
	address.sin_port = htons(m_port);				// 端口号

	int flag = 1;
	// 允许重用本地地址和端口
	setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	// 传统绑定步骤
	ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
	// >= 0的设定，因为只有小于0才是错误情况
	assert(ret >= 0);
	// 传统监听步骤
	ret = listen(m_listenfd, 5);
	assert(ret >= 0);

	utils.init(TIMESLOT);												// 初始化客户端数据的最小超时单位

	// epoll创建内核事件表
	epoll_event events[MAX_EVENT_NUMBER];
	m_epollfd = epoll_create(6);								// 创建一个指示epoll内核事件表的文件描述符
	assert(m_epollfd != -1);

	// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
	utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
	http_conn::m_epollfd = m_epollfd;

	// 创建管道套接字
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
	assert(ret != -1);

	// 设置管道写端为非阻塞，为什么写端要非阻塞？
	// send是将信息发送给套接字缓冲区，如果缓冲区满了，则会阻塞，
	// 这时候会进一步增加信号处理函数的执行时间，为此，将其修改为非阻塞。
	utils.setnonblocking(m_pipefd[1]);							// 对文件描述符设置为非阻塞

	// 设置管道读端为ET非阻塞，统一事件源
	utils.addfd(m_epollfd, m_pipefd[0], false, 0);

	// 设置信号函数
	utils.addsig(SIGPIPE, SIG_IGN);
	// 传递给主循环的信号值，这里只关注SIGNALRM和SIGTERM
	utils.addsig(SIGALRM, utils.sig_handler, false);
	utils.addsig(SIGTERM, utils.sig_handler, false);

	// 每隔TIMESLOT时间触发SIGALRM信号
	alarm(TIMESLOT);

	// 工具类，信号和描述符基础操作
	Utils::u_pipefd = m_pipefd;
	Utils::u_epollfd = m_epollfd;
}

// 定时器
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
	users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

	// 初始化client_data数据
	users_timer[connfd].address = client_address;
	users_timer[connfd].sockfd = connfd;
	// 创建定时器，绑定用户数据和设置回调函数
	util_timer *timer = new util_timer;
	timer->user_data = &users_timer[connfd];
	timer->cb_func = cb_func;
	// 设置定时器失效时间，即超时时间，并将定时器添加到链表中
	time_t cur = time(NULL);
	timer->expire = cur + 3 * TIMESLOT;
	users_timer[connfd].timer = timer;
	utils.m_timer_lst.add_timer(timer);
}

// 若有数据传输，则将定时器往后延迟3个单位
// 并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
	time_t cur = time(NULL);
	timer->expire = cur + 3 * TIMESLOT;
	utils.m_timer_lst.adjust_timer(timer);

	LOG_INFO("%s", "adjust timer once");
}

// 处理定时器
void WebServer::deal_timer(util_timer *timer, int sockfd)
{
	// 处理定时器的回调函数，并在定时器列表中删除
	timer->cb_func(&users_timer[sockfd]);
	if (timer)
	{
		utils.m_timer_lst.del_timer(timer);
	}

	// 在日志中写入关闭的fd
	LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

// 处理客户端数据
bool WebServer::dealclientdata()
{
	struct sockaddr_in client_address;
	socklen_t client_addrlength = sizeof(client_address);
	// 判断监听模式
	if (0 == m_LISTENTrigmode)
	{
		// 传统网络编程，接受连接
		int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
		if (connfd < 0)
		{
			LOG_ERROR("%s:errno is:%d", "accept error", errno);
			return false;
		}

		// 判断http的用户数量是否大于最大值
		if (http_conn::m_user_count >= MAX_FD)
		{
			// 大于，则显示错误，服务器繁忙
			utils.show_error(connfd, "Internal server busy");
			LOG_ERROR("%s", "Internal server busy");
			return false;
		}

		// 连接成功，绑定一个定时器
		timer(connfd, client_address);
	}
	else
	{
		while (1)
		{
			// 接受连接
			int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
			if (connfd < 0)
			{
				LOG_ERROR("%s:errno is:%d", "accept errno", errno);
				break;
			}

			// 判断http的用户数量是否大于最大值
			if (http_conn::m_user_count >= MAX_FD)
			{
				// 大于，则显示错误，服务器繁忙
				utils.show_error(connfd, "Internal server busy");
				LOG_ERROR("%s", "Internal server busy");
				return false;
			}

			// 连接成功，绑定一个定时器
			timer(connfd, client_address);
		}

		return false;
	}

	return true;
}

// 处理信号
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
	int ret = 0;
	int sig;
	char signals[1024];
	// 接受信号
	ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
	if (ret == -1)
	{
		return false;
	}
	else if (ret == 0)
	{
		return false;
	}
	else
	{
		for (int i = 0; i < ret; ++i)
		{
			switch (signals[i])
			{
			case SIGALRM:
			{
				timeout = true;
				break;
			}
			case SIGTERM:
			{
				stop_server = true;
				break;
			}
			}
		}
	}

	return true;
}

// 处理读事件
void WebServer::dealwithread(int sockfd)
{
	// sockfd对应的定时器
	util_timer *timer = users_timer[sockfd].timer;

	// reactor模式
	if (1 == m_actormodel)
	{
		// 如果定时器不为0，则调整
		if (timer)
		{
			adjust_timer(timer);
		}

		// 若监测到读事件，将该事件放入请求队列
		m_pool->append(users + sockfd, 0);

		// 等待到执行完成后，才能执行下一条请求
		while (true)
		{
			if (1 == users[sockfd].improv)
			{
				if (1 == users[sockfd].timer_flag)
				{
					deal_timer(timer, sockfd);
					users[sockfd].timer_flag = 0;
				}

				users[sockfd].improv = 0;
				break;
			}
		}
	}
	else
	{
		// proactor模式
		if (users[sockfd].read_once())
		{
			LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

			// 若监测到读事件，将该事件放入请求队列
			m_pool->append_p(users + sockfd);

			if (timer)
			{
				adjust_timer(timer);
			}
		}
		else
		{
			deal_timer(timer, sockfd);
		}
	}
}

// 处理写事件
void WebServer::dealwithwrite(int sockfd)
{
	util_timer *timer = users_timer[sockfd].timer;
	
	// reactor模式
	if (1 == m_actormodel)
	{
		if (timer)
		{
			adjust_timer(timer);
		}

		m_pool->append(users + sockfd, 1);

		while (true)
		{
			if (1 == users[sockfd].improv)
			{
				if (1 == users[sockfd].timer_flag)
				{
					deal_timer(timer, sockfd);
					users[sockfd].timer_flag = 0;
				}

				users[sockfd].improv = 0;
				break;
			}
		}
	}
	else
	{
		// proactor模式
		if (users[sockfd].write())
		{
			LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

			if (timer)
			{
				adjust_timer(timer);
			}
		}
		else
		{
			deal_timer(timer, sockfd);
		}
	}
}

// 事件循环
void WebServer::eventLoop()
{
	bool timeout = false;						// 是否超时
	bool stop_server = false;					// 是否停止服务器

	while (!stop_server)
	{
		// 等待事件
		int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
		if (number < 0 && errno != EINTR)
		{
			LOG_ERROR("%s", "epoll failure");
			break;
		}

		for (int i = 0; i < number; i++)
		{
			// 获得事件的fd
			int sockfd = events[i].data.fd;

			// 处理新到的客户连接
			if (sockfd == m_listenfd)
			{
				// 处理客户端的数据
				bool flag = dealclientdata();
				if (false == flag)
					continue;
			}
			else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
			{
				// 服务器端关闭连接，移除对应的定时器
				util_timer *timer = users_timer[sockfd].timer;
				deal_timer(timer, sockfd);
			}
			// 处理信号
			else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
			{
				bool flag = dealwithsignal(timeout, stop_server);
				if (false == flag)
					LOG_ERROR("%s", "dealclientdata failure");
			}
			// 处理客户连接上接收到的数据
			else if (events[i].events & EPOLLIN)
			{
				dealwithread(sockfd);
			}
			else if (events[i].events & EPOLLOUT)
			{
				dealwithwrite(sockfd);
			}
		}

		// 判断是否超时
		if (timeout)
		{
			// 定时处理任务
			utils.timer_handler();

			LOG_INFO("%s", "timer tick");

			timeout = false;
		}
	}
}

