#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

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
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

// http连接类
class http_conn
{
public:
	static const int FILENAME_LEN		= 200;		// 文件名长度
	static const int READ_BUFFER_SIZE	= 2048;		// 读取的缓存大小
	static const int WRITE_BUFFER_SIZE	= 1024;		// 写入的缓存大小

	// 枚举方法
	enum METHOD
	{
		GET = 0,						// 请求指定的页面信息，并返回实体主体
		POST,							// 类似于get请求，只不过返回的响应中没有具体的内容，用于获取报头
		HEAD,							// 向指定资源提交数据进行处理请求(例如提交表单或者上传文件)
		PUT,							// 从客户端向服务器传送的数据取代指定的文档的内容
		DELETE,							// 请求服务器删除指定的页面
		TRACE,							// 回显服务器收到的请求，主要用于测试或诊断
		OPTIONS,						// 允许客户端查看服务器的性能
		CONNECT,						// HTTP/1.1协议中预留给能够将连接改为管道方式的代理服务器
		PATH							// 路径
	};

	// 枚举：检查状态
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,	// 请求行
		CHECK_STATE_HEADER,				// 请求头部
		CHECK_STATE_CONTENT				// 请求数据
	};

	// HTTP状态码
	enum HTTP_CODE
	{
		NO_REQUEST,						// 没有请求
		GET_REQUEST,					// GET请求
		BAD_REQUEST,					// BAD请求
		NO_RESOURCE,					// 没有资源
		FORBIDDEN_REQUEST,				// 禁止请求
		FILE_REQUEST,					// 文件请求
		INTERNAL_ERROR,					// 网络错误
		CLOSED_CONNECTION				// 关闭连接
	};

	// 枚举：请求行和状态行的状况
	enum LINE_STATUS
	{
		LINE_OK = 0,					// 成功
		LINE_BAD,						// 失败
		LINE_OPEN						// 打开
	};

public:
	// 构造函数和析构函数
	http_conn() {}
	~http_conn() {}

public:
	// 初始化连接
	void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
	// 关闭连接
	void close_conn(bool real_close = true);
	// 处理过程函数
	void process();
	// 读取一次
	bool read_once();
	// 写入
	bool write();

	// 获得地址
	sockaddr_in* get_address()
	{
		return &m_address;
	}

	// 初始化与数据库连接池的连接
	void initmysql_result(connection_pool *connPool);

	// 时间标志
	int timer_flag;
	int improv;

private:
	void init();											// 初始化

	HTTP_CODE process_read();								// 读取过程
	bool process_write(HTTP_CODE ret);						// 写入过程

	HTTP_CODE parse_request_line(char *text);				// 解析请求行
	HTTP_CODE parse_headers(char *text);					// 解析请求头部
	HTTP_CODE parse_content(char *text);					// 解析请求数据
	HTTP_CODE do_request();									// 处理请求
	char* get_line() { return m_read_buf + m_start_line; };	// 获取行
	LINE_STATUS parse_line();								// 解析行
	void unmap();											// 

	bool add_response(const char *fromat, ...);				// 增加响应，通过以下函数构造一个响应
	bool add_content(const char *content);					// 增加响应体
	bool add_status_line(int status, const char *title);	// 增加响应行
	bool add_headers(int content_length);					// 增加响应头
	bool add_content_type();								// 增加响应内容的类型
	bool add_content_length(int content_length);			// 增减响应内容的长度
	bool add_linger();										// 增加 是否连接
	bool add_blank_line();									// 增加空白行

public:
	static int m_epollfd;									// epollfd句柄
	static int m_user_count;								// 用户数量
	MYSQL *mysql;											// 数据库
	int m_state;											// 读为0，写为1

private:
	int m_sockfd;											// 套接字句柄
	sockaddr_in m_address;									// 地址

	char m_read_buf[READ_BUFFER_SIZE];						// 读取的缓存大小
	int  m_read_idx;										// 读取索引
	int  m_checked_idx;										// 检查索引
	int  m_start_line;										// 初始行

	char m_write_buf[WRITE_BUFFER_SIZE];					// 写入的缓存大小
	int  m_write_idx;										// 写入的索引
	CHECK_STATE m_check_state;								// 检查状态
	METHOD m_method;										// 请求方法
	char m_real_file[FILENAME_LEN];							// 真实的文件

	char *m_url;											// 网址
	char *m_version;										// 版本号
	char *m_host;											// 主机号
	int  m_content_length;									// 内容长度
	bool m_linger;											// 
	char *m_file_address;									// 文件地址
	struct stat m_file_stat;								// 文件统计
	struct iovec m_iv[2];									// 向量元素
	int m_iv_count;											//
	int cgi;												// 是否启用POST
	char *m_string;											// 存储请求头数据
	int bytes_to_send;										// 要发送的字节
	int bytes_have_send;									// 字节已发送
	char *doc_root;											// 文件的根目录

	map<string, string> m_users;							// 用户名、密码
	int m_TRIGMode;											// 触发模式
	int m_close_log;										// 日志开关

	char sql_user[100];										// 数据库用户名
	char sql_passwd[100];									// 数据库密码
	char sql_name[100];										// 数据库名
};

#endif // HTTPCONNECTION_H

