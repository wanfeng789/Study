#include "Server.h"


//16进制数转化为10进制, return 0不会出现
static int hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}
static void Decode(char* to, char* from)
{
	while (*from != '\0')
	{
		//依次判断from中 %20 三个字符
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			//字符串E8变成了真正的16进制的E8
			*to = hexit(from[1]) * 16 + hexit(from[2]);
			//移过已经处理的两个字符(%21指针指向1),
			// 表达式3的++from还会再向后移一个字符
			from += 2;
		}
		else
		{
			*to = *from;
		}
		++to;
		++from;
	}
	*to = '\0';
}

const char* getFileType(char* filename)
{
	char* dot = strrchr(filename, '.');
	if (dot == (char*)0)
		return "text/plain; charset=utf-8";
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";

}
Server::Server(unsigned short port)
{
	this->port = port;
}

int Server::Serverstart()
{
	// 设置监听的状态
	setListen();

	// 检测连接的状态
	epfd = epoll_create(1);
	// 将监听的文件描述符添加到epoll树上
	setEpollAdd(lfd);

	// 循环检测
	struct epoll_event evs[1024];
	int evsLen = sizeof(evs) / sizeof(evs[0]);
	// 循环结束的标志
	bool flag = 1;
	while (flag)
	{
		flag = 0;
		// 循环检测epoll树上文件描述符的变化
		// 采用多线程处理加快速度
		// 主线程处理客户端连接, 子线程进行处理请求和发送
		int num = epoll_wait(this->epfd, evs, evsLen, -1);
		pthread_t pfd;
		for (int i = 0; i < num; ++i)
		{
			flag = 1;
			int curfd = evs[i].data.fd;
			
			if (curfd == lfd)
			{
				// 此时有新的客户端连接
				std::cout << "客户端连接" << std::endl;
				pthread_create(&pfd, NULL, acceptClient, this);
				// 进行线程分离自动回收线程
				pthread_detach(pfd);
			}
			else
			{
				// 处理请求进行传输数据
				std::cout << "处理请求" << std::endl;
				httpRSMsg();
			}
		}

	}
	// 关闭监听的文件描述符
	disConnect(lfd);
	return 0;
}

int Server::setListen()
{
	int ret = 0;
	// 创建监听的套接字
	this->lfd = socket(AF_INET, SOCK_STREAM, 0);
	
	// 绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	// 在绑定之前设置端口复用
	int val = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1)
	{
		std::cout << "绑定失败" << std::endl;
		return -1;
	}
	ret = listen(lfd, 1024);
	if (ret == -1)
	{
		std::cout << "绑定失败" << std::endl;
		return -1;
	}	
	return 0;
}

int Server::setEpollAdd(int fd)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	// 设置为边沿模式
	ev.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	return 0;
}

void* acceptClient(void* arg)
{
	Server* s = (Server*)arg;
	// 通讯的文件描述符
	int cfd = accept(s->lfd, NULL, NULL);
	// 将通讯的文件描述设置为非阻塞模式
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, flag, F_SETFL);
	// 添加到epoll树上
	s->m_list.insert({ pthread_self(), cfd});
	return 0;
}


int Server::httpRSMsg()
{
	int cfd = pthread_self();
	// 先接受客户端的请求的数据
	recvMsg();
	// 向客户端发送响应数据
	sendMsg();
	return 0;
}

int Server::recvMsg()
{
	int cfd = m_list[pthread_self()];
	// 循环接受数据, 数据可能过大需要进行判断处理
	char buf[4096];		// 接受的总数据
	char tmp[1024];		// 临时数据
	memset(buf, 0, sizeof(buf));
	int len = 0;
	int ptr = 0;
	while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
	{
		if ((len + ptr) < sizeof(buf))
		{
			memcpy(buf + ptr, tmp, len);
		}
		ptr += len;

	}
	// 判断收到的数据情况
	if (len == -1 && errno == EAGAIN)
	{
		// 取出请求行
		char* p = strstr(buf, "\r\n");
		int index = buf - p;
		// 进行数据的截断
		buf[index] = '\0';

		// 解析请求行的数据
		parseRequestLine(buf);
	}
	else if (len == 0)
	{
		// 客户端断开了连接
		std::cout << "客户端断开连接" << std::endl;
		disConnect(cfd);
	}
	else
	{
		std::cout << "接受数据错误" << std::endl;
		return -1;
	}
	return 0;
}

int Server::sendMsg()
{
	// 获得文件的属性
	struct stat st;
	int ret = stat(this->file, &st);
	/*
		响应的数据包括:
		状态行
		响应头
		空行
		响应的数据

	*/
	if (ret == -1)
	{
		// 表明请求的文件不存在, 发送404 失败界面
		this->file = "404.html";
		this->descStatus = "Not Found";
		this->status = 404;
		this->fileType = (char*)getFileType("./html");
		this->fileSize = -1;

		sendHeadMsg();
		sendTrueMsg(1);
	}
	// 这里请求的目录
	if (S_ISDIR(st.st_mode))
	{
		// 发送头
		this->file = file;
		this->descStatus = "OK";
		this->status = 200;
		this->fileType = (char*)getFileType(".html");
		this->fileSize = -1;
		// 发送数据
		sendTrueMsg(1.0);
	}
	else
	{
		// 则是文件
		this->file = file;
		this->descStatus = "OK";
		this->status = 200;
		this->fileType = (char*)getFileType(file);
		this->fileSize = st.st_size;
		// 发送头
		sendHeadMsg();
		// 发送数据
		sendTrueMsg(1);
	}
	return 0;
}

Server::~Server()
{
}

int Server::disConnect(int cfd)
{
	// 将cfd从epoll树上摘除
	epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);

	// 关闭通讯的文件描述符
	close(cfd);

}
int Server::parseRequestLine(const char* buf)
{
	int cfd = m_list[pthread_self()];
	/*
		请求的方法  请求的资源目录 请求的数据
		
	*/
	// 请求的方法
	char method[10];
	// 请求的资源根目录
	char reqsrc[20];
	memset(method, 0, sizeof(method));
	memset(reqsrc, 0, sizeof(reqsrc));
	
	// 使用 sscanf 取出对应的方法
	sscanf(buf, "[^ ] [^ ]", method, reqsrc);

	// 判断请求的方法是不是  get
	if (strcasecmp(method, "get") == 0)
	{
		std::cout << "请求的方法正确" << std::endl;
	}
	else
	{
		std::cout << "请求方法错误" << std::endl;
		return -1;
	}
	// 查询请求的资源目录
	char* file = NULL;
	// 解决中文问题
	Decode(reqsrc, reqsrc);
	if (strcmp(reqsrc, "\n") == 0)
	{
		// 请求的是资源根目录
		file = "./";
	}
	else
	{
		// 这里则是访问的根目录下面的资源
		file = reqsrc + 1;
	}
	strcpy(this->file, file);
	return 0;
}
int Server::sendHeadMsg()
{
	int cfd = m_list[pthread_self()];
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	/*
		只需要发这三种就可以, 因为TCP是流式传输
		状态行 --------> http/1.1 状态码  描述
		响应头
		空行
	*/
	// 状态行
	sprintf(buf, "http/1.1 %d %s\r\n", this->status, this->descStatus);
	// 响应头
	sprintf(buf + strlen(buf), "Content-Type:%d Content-Length:%d\r\n", this->fileType, this->fileSize);
	// 空行
	sprintf(buf + strlen(buf), "\r\n");
	send(cfd, buf, sizeof(buf), 0);

}
int Server::sendTrueMsg(int num)
{
	int cfd = m_list[pthread_self()];
	int fd = open(this->file, O_RDONLY);
	if (fd == -1)
	{
		std::cout << "打开文件失败" << std::endl;
		return -1;
	}
	char buf[1024];
	while (1)
	{
		memset(buf, 0, sizeof(buf));
		int len = read(fd, buf, sizeof(buf));
		if (len > 0)
		{
			// 读到了数据
			// 读多少就发多少
			send(cfd, buf, len, 0);
			// 浏览器接受速率慢, 延迟
			usleep(100);
		}
		else if (len == 0)
		{
			// 数据读完结束
			break;
		}
		else
		{
			std::cout << "读取数据失败" << std::endl;
			return -1;
		}
	}

	close(fd);
}
int Server::sendTrueMsg(double num)
{
	int cfd = m_list[pthread_self()];
	

}
