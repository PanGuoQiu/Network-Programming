#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

// 定义http响应的一些状态信息
const char *ok_200_title	= "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form	= "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form	= "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form	= "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form	= "There was an unusual problem serving the request file.\n";

// 互斥锁和用户名、密码映射
locker m_lock;
map<string, string> users;

// 初始化与数据库连接池的连接
void http_conn::initmysql_result(connection_pool *connPool)
{
	// 先从连接池中取一个连接
	MYSQL *mysql = NULL;
	connectionRAII mysqlcon(&mysql, connPool);					// 构造时，自动连接数据库连接池

	// 在user表中检索username, passwd数据，浏览器端输入
	if (mysql_query(mysql, "SELECT username,passwd FROM user"))
	{
		LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
	}

	// 从表中检索完整的结果集
	MYSQL_RES *result = mysql_store_result(mysql);

	// 返回结果集中的列数
	int num_fields = mysql_num_fields(result);

	// 返回所有字段结构的数组
	MYSQL_FIELD *fields = mysql_fetch_fields(result);

	// 从结果集中获取下一行，将对应的用户名和密码，存入map中
	while (MYSQL_ROW row = mysql_fetch_row(result))
	{
		string temp1(row[0]);
		string temp2(row[1]);
		users[temp1] = temp2;
	}
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);

	return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode)
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	else
		event.events = EPOLLIN | EPOLLRDHUP;

	if (one_shot)
		event.events |= EPOLLONESHOT;

	// 将内核事件表注册为读事件
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	// 对文件描述符设置为非阻塞
	setnonblocking(fd);
}

// 从内核事件表删除描述符
void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode)
		event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	else
		event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

	// 将事件重置为EPOLLONESHOT
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化用户数量和epollfd事件
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 关闭连接，关闭一个连接，客户总量减1
void http_conn::close_conn(bool real_close)
{
	// 判断是否真实关闭且没有套接字句柄
	if (real_close && (m_sockfd != -1))
	{
		printf("close %d\n", m_sockfd);				// 输出关闭的套接字句柄号
		removefd(m_epollfd, m_sockfd);				// 从内核事件表删除描述符
		m_sockfd = -1;								// 设置为无效
		m_user_count--;								// 客户减一
	}
}

// 初始化连接，外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
					 int close_log, string user, string passwd, string sqlname)
{
	// 套接字句柄和地址
	m_sockfd = sockfd;
	m_address = addr;

	// 在内核表中注册事件，用户加1
	addfd(m_epollfd, sockfd, true, m_TRIGMode);
	m_user_count++;

	// 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中年内容完全为空
	doc_root = root;								// 根目录
	m_TRIGMode = TRIGMode;							// 触发模式
	m_close_log = close_log;						// 日志开关

	// 拷贝获得：用户名、密码和数据库名
	strcpy(sql_user, user.c_str());
	strcpy(sql_passwd, passwd.c_str());
	strcpy(sql_name, sqlname.c_str());

	// 初始化新接受的连接
	init();
}

// 初始化新接受的连接
// check_state默认为分析请求行状态
void http_conn::init()
{
	mysql = NULL;
	bytes_to_send = 0;
	bytes_have_send = 0;
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;
	m_write_idx = 0;
	cgi = 0;
	m_state = 0;
	timer_flag = 0;
	improv = 0;

	// 设置读缓存、写缓存和文件名缓存
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

// 从状态机，用于分析一行内容
// 返回值为行的读取状态，有LINE_OK, LINE_BAD, LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
	char temp;
	// 从检查索引开始，遍历一行
	for (; m_checked_idx < m_read_idx; ++m_checked_idx)
	{
		temp = m_read_buf[m_checked_idx];
		// 判断字符是否为“回车”键，如果是，则确定为一行，返回行的状态
		if (temp == '\r')
		{
			if ((m_checked_idx + 1) == m_read_idx)
				return LINE_OPEN;
			else if (m_read_buf[m_checked_idx + 1] == '\n')
			{
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}

			return LINE_BAD;
		}
		// 判断字符是否为“换行”键，如果是，则确定为一行，返回行的状态
		else if (temp == '\n')
		{
			if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
			{
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}

			return LINE_BAD;
		}
	}

	return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
	if (m_read_idx >= READ_BUFFER_SIZE)
	{
		return false;
	}

	int bytes_read = 0;

	// LT读取数据
	if (0 == m_TRIGMode)
	{
		// 接受数据到m_read_buf中
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		m_read_idx += bytes_read;

		if (bytes_read <= 0)
		{
			return false;
		}

		return true;
	}
	// ET读数据
	else
	{
		while (true)
		{
			// 接受数据到m_read_buf中
			bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
			if (bytes_read == -1)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					break;

				return false;
			}
			else if (bytes_read == 0)
			{
				return false;
			}

			// 更新读取索引
			m_read_idx += bytes_read;
		}

		return true;
	}
}

// 解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
	// 查找到"\t"字符时的字符串的位置
	m_url = strpbrk(text, " \t");
	if (!m_url)
	{
		return BAD_REQUEST;
	}

	// 判断请求行的请求方法
	*m_url++ = '\0';
	char *method = text;
	if (strcasecmp(method, "GET") == 0)
		m_method = GET;
	else if (strcasecmp(method, "POST") == 0)
	{
		m_method = POST;
		cgi = 1;									// 启动POST请求
	}
	else
		return BAD_REQUEST;

	// 检索版本号
	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if (!m_version)
		return BAD_REQUEST;
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
		return BAD_REQUEST;
	if (strncasecmp(m_url, "http://", 7) == 0)
	{
		m_url += 7;
		m_url = strchr(m_url, '/');
	}

	if (strncasecmp(m_url, "https://", 8) == 0)
	{
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	if (!m_url || m_url[0] != '/')
		return BAD_REQUEST;
	// 当url为/时，显示判断界面
	if (strlen(m_url) == 1)
		strcat(m_url, "judge.html");

	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

// 解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
	if (text[0] == '\0')
	{
		if (m_content_length != 0)
		{
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}

		return GET_REQUEST;
	}
	// 请求头的连接信息
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "Keep-alive") == 0)
		{
			m_linger = true;
		}
	}
	// 请求头的内容长度
	else if (strncasecmp(text, "Content-length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	// 请求头的主机号
	else if (strncasecmp(text, "Host:", 5) == 0)
	{
		text += 5;
		text += strspn(text, " \t");
		m_host = text;
	}
	else
	{
		LOG_INFO("oop!unknow header: %s", text);
	}

	return NO_REQUEST;
}

// 判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
	// 判断读取索引是否大于检查索引+内容长度
	if (m_read_idx >= (m_content_length + m_checked_idx))
	{
		text[m_content_length] = '\0';
		// POST请求中最后为输入的用户名和密码
		m_string = text;
		return GET_REQUEST;
	}

	return NO_REQUEST;
}

// 有限状态机处理请求报文
http_conn::HTTP_CODE http_conn::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char *text = 0;

	/*
	从状态机读取数据
	调用get_line函数，通过m_start_line将从状态机读取数据间接赋给text
	主状态机解析text
	*/
	while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
	{
		text = get_line();
		m_start_line = m_checked_idx;
		LOG_INFO("%s", text);
		// 主状态机
		switch (m_check_state)
		{
		case CHECK_STATE_REQUESTLINE:						// 主状态机：检查状态是否为请求行
		{
			ret = parse_request_line(text);					// 从状态机：解析请求行
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			break;
		}
		case CHECK_STATE_HEADER:							// 主状态机：检查状态是否为请求头
		{
			ret = parse_headers(text);						// 从状态机：解析请求头
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			else if (ret == GET_REQUEST)
			{
				return do_request();						// 如果有GET请求，则调用功能逻辑单元
			}
			break;
		}
		case CHECK_STATE_CONTENT:							// 主状态机：检查状态是否为消息体
		{
			ret = parse_content(text);						// 从状态机：解析消息体
			if (ret == GET_REQUEST)
				return do_request();

			line_status = LINE_OPEN;
			break;
		}
		default:
			return INTERNAL_ERROR;
		}
	}

	return NO_REQUEST;
}

// 功能处理单元
http_conn::HTTP_CODE http_conn::do_request()
{
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);
	//printf("m_url:%s\n", m_url);
	const char *p = strrchr(m_url, '/');

	// 处理cgi，POST请求
	if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
	{
		// 根据标志判断是登录检测还是注册检测
		char flag = m_url[1];

		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/");
		strcat(m_url_real, m_url + 2);
		strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
		free(m_url_real);

		// 将用户名和密码提取出来
		// user = 123 && passwd = 123
		char name[100], password[100];
		int i;
		for (i = 5; m_string[i] != '&'; ++i)
			name[i - 5] = m_string[i];
		name[i - 5] = '\0';

		int j = 0;
		for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
			password[j] = m_string[i];
		password[j] = '\0';

		if (*(p + 1) == '3')
		{
			// 如果是注册，先检测数据库中是否有重名的
			// 没有重名的，进行增加数据
			char *sql_insert = (char *)malloc(sizeof(char) * 200);
			// 构成一条sql插入语句
			strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
			strcat(sql_insert, "'");
			strcat(sql_insert, name);
			strcat(sql_insert, "', '");
			strcat(sql_insert, password);
			strcat(sql_insert, "')");

			// 在表中查找是否有重名的，并用户名和密码映射到内存中
			if (users.find(name) == users.end())
			{
				m_lock.lock();
				int res = mysql_query(mysql, sql_insert);
				users.insert(pair<string, string>(name, password));
				m_lock.unlock();

				// 判断sql语句是否执行成功，并调用相应的页面
				if (!res)
					strcpy(m_url, "/log.html");
				else
					strcpy(m_url, "/registerError.html");
			}
			else
				strcpy(m_url, "/registerError.html");
		}
		// 如果是登录，直接判断
		// 若浏览器输入的用户名和密码在表中可以查找到，返回1，否则返回0
		else if (*(p + 1) == '2')
		{
			if (users.find(name) != users.end() && users[name] == password)
				strcpy(m_url, "/welcome.html");
			else
				strcpy(m_url, "/logError.html");
		}
	}

	if (*(p + 1) == '0')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/register.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '1')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/log.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '5')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/picture.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '6')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/video.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '7')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/fans.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else
		strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

	// 统计
	if (stat(m_real_file, &m_file_stat) < 0)
		return NO_RESOURCE;

	// 禁止
	if (!(m_file_stat.st_mode & S_IROTH))
		return FORBIDDEN_REQUEST;

	// 失败
	if (S_ISDIR(m_file_stat.st_mode))
		return BAD_REQUEST;

	// 打开文件，获得地址并返回文件请求
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	return FILE_REQUEST;
}

// 
void http_conn::unmap()
{
	// 判断文件地址是否为空
	if (m_file_address)
	{
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}

// 写入是否成功，响应是否成功
bool http_conn::write()
{
	int temp = 0;

	// 判断要传送的字节是否为0
	if (bytes_to_send == 0)
	{
		// 重置事件为EPOLLONESHOT，并初始化
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
		init();
		return true;
	}

	while (1)
	{
		// writev()代表集中写，即将多块分散的内存一并写入文件描述符中
		temp = writev(m_sockfd, m_iv, m_iv_count);
		// 判断写入的字节数
		if (temp < 0)
		{
			if (errno == EAGAIN)
			{
				// 重置事件为EPOLLONESHOT
				modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
				return true;
			}

			// 无映射，返回错误
			unmap();
			return false;
		}

		bytes_have_send += temp;					// 已发送的字节+写入的字节数
		bytes_to_send -= temp;						// 要发送的字节-写入的字节数
		// 判断已发送的字节数是否大于实际写入的大小
		if (bytes_have_send >= m_iv[0].iov_len)
		{
			m_iv[0].iov_len = 0;
			// 将要发送数据的缓存
			m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
			m_iv[1].iov_len = bytes_to_send;
		}
		else
		{
			m_iv[0].iov_base = m_write_buf + bytes_have_send;		// 将要发送数据的缓存
			m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;	// 实际写入的长度
		}

		// 判断要发送的字节小于0
		if (bytes_to_send <= 0)
		{
			// 无映射，并将事件设置为EPOLLONESHOT读事件
			unmap();
			modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

			if (m_linger)
			{
				init();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}


// 响应
bool http_conn::add_response(const char *format, ...)
{
	// 如果写入索引>=写入缓存大小，则错误
	if (m_write_idx >= WRITE_BUFFER_SIZE)
		return false;

	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
	{
		va_end(arg_list);
		return false;
	}

	m_write_idx += len;
	va_end(arg_list);

	// 在日志信息中写入请求信息
	LOG_INFO("request:%s", m_write_buf);

	return true;
}

// 响应的状态行
bool http_conn::add_status_line(int status, const char *title)
{
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 响应的消息头
bool http_conn::add_headers(int content_len)
{
	return add_content_length(content_len) && add_linger() && add_blank_line();
}

// 响应内容的长度
bool http_conn::add_content_length(int content_len)
{
	return add_response("Content-Length:%d\r\n", content_len);
}

// 响应内容的类型
bool http_conn::add_content_type()
{
	return add_response("Content-Type:%s\r\n", "text/html");
}

// 连接：继续连接 或 关闭
bool http_conn::add_linger()
{
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 空白行
bool http_conn::add_blank_line()
{
	return add_response("%s", "\r\n");
}

// 内容
bool http_conn::add_content(const char *content)
{
	return add_response("%s", content);
}

// 根据请求状态码处理响应的写入
bool http_conn::process_write(HTTP_CODE ret)
{
	switch (ret)
	{
	case INTERNAL_ERROR:							// 网络错误
	{
		add_status_line(500, error_500_title);		// 响应的状态行
		add_headers(strlen(error_500_form));		// 响应的消息头
		if (!add_content(error_500_form))			// 如果没有正文，则错误
			return false;
		break;
	}
	case BAD_REQUEST:								// 失败请求
	{
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	}
	case FORBIDDEN_REQUEST:							// 禁止访问
	{
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form))
			return false;
		break;
	}
	case FILE_REQUEST:								// 文件请求
	{
		add_status_line(200, ok_200_title);			// 成功处理请求
		if (m_file_stat.st_size != 0)				// 判断统计文件的大小
		{
			add_headers(m_file_stat.st_size);
			m_iv[0].iov_base = m_write_buf;
			m_iv[0].iov_len  = m_write_idx;
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len  = m_file_stat.st_size;
			m_iv_count = 2;
			bytes_to_send = m_write_idx + m_file_stat.st_size;
			return true;
		}
		else
		{
			const char *ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
				return false;
		}
	}
	default:
		return false;
	}

	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	bytes_to_send = m_write_idx;
	
	return true;
}

//处理http报文请求与报文响应
void http_conn::process()
{
	//NO_REQUEST，表示请求不完整，需要继续接收请求数据
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST)
	{
		//注册并监听读事件
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
		return;
	}

	//调用process_write完成报文响应
	bool write_ret = process_write(read_ret);
	if (!write_ret)
	{
		close_conn();
	}

	// 注册并监听事件
	modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

