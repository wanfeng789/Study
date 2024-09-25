#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>
#include <utime.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/dir.h>
class Server
{
public:
	Server(unsigned short port);

	// 启动服务器
	int Serverstart();
	// 设置监听的状态
	int setListen();
	// 断开连接
	int disConnect();
	// 解析请求行的数据
	int parseRequestLine(const char* buf);

	// 等待客户端连接
	int acceptClient();

	~Server();
public:
	//  接受发送消息  作为回调函数
	int httpRSMsg();
	// 接受请求数据
	 int recvMsg();
	// 发送响应的数据
	 int sendMsg();
	// 将节点添加到epoll树上
	 int setEpollAdd(int fd);
	 // 发送头数据
	 int sendHeadMsg();
	 // 发送真正的数据  规定 double 为目录  int 为文件
	 int sendTrueMsg(double num);
	 int sendTrueMsg(int num);
private:
	unsigned short port;		// 端口号
	 int epfd;					// epoll树根
	int lfd;					// 监听的文件描述
	int cfd;					// 通讯的文件描述符
	int status;					// 状态码
	 const char* descStatus;		// 对状态码的描述
	 const char* fileType;				// 发送文件的类型
	 char* file;					// 请求的路径
	int fileSize;				// 发送文件的大小
};