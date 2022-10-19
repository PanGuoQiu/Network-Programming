#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

// 配置服务器类
class Config
{
public:
	// 构造函数和析构函数
	Config();
	~Config() {}

	// 解析命令
	void parse_arg(int argc, char *argv[]);

	// 控制的变量
	int PORT;								// 端口号
	int LOGWrite;							// 日志写入方式
	int TRIGMode;							// 触发组合模式
	int LISTENTrigmode;						// listenfd触发模式
	int CONNTrigmode;						// connfd触发模式
	int OPT_LINGER;							// 优雅关闭连接
	int sql_num;							// 数据库连接池数量
	int thread_num;							// 线程池内的线程数量
	int close_log;							// 日志开关
	int actor_model;						// 并发模型选择
};

#endif // CONFIG_H
