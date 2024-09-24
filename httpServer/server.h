#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <error.h>
#include <errno.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <strings.h>
#include <dirent.h>
// 初始化监听套接字
int initListenFd(unsigned short port);

// 启动 epoll 模型
int epollRun(unsigned short port);

// 建立新连接
int acceptConn(int lfd, int epfd);

// 接受数据
int recvHttpRequest(int cfd, int epfd);
// 解析请求行
int parseRequestLine(const char* reqpath, int cfd, int epfd);
// 发送响应消息的前三部分
int sendHeadMsg(int cfd, int status, const char* descr,
				const char* type, int length);
// 将文件内容发送
int sendFile(int cfd, const char* file);

// 断开连接
int disConnect(int cfd, int epfd);

// 获取文件的格式
const char* getFileType(const char* filename);

// 发送目录的信息
int sendDirMsg(int cfd, const char* dir);


// 转码操作
int hexit(char c);
void Decode(char* to, char* from);