#include "Server.h"

//16进制数转化为10进制, return 0不会出现
int hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}
void Decode(char* to, char* from)
{
	while(*from != '\0') 
	{
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //依次判断from中 %20 三个字符

			*to = hexit(from[1]) * 16 + hexit(from[2]);//字符串E8变成了真正的16进制的E8
			from += 2;                      //移过已经处理的两个字符(%21指针指向1),表达式3的++from还会再向后移一个字符
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
	
	this->epfd = 0;
	this->lfd = 0;
	this->cfd = 0;

	this->status = 0;
	this->descStatus = NULL;
	this->fileType = NULL;
	this->file = NULL;
	this->fileSize = 0;
}

int Server::Serverstart()
{
	// 设置监听的状态
	setListen();

	// 检测连接的状态
	epfd = epoll_create(1);
	// 将监听的文件描述符添加到epoll树上
	setEpollAdd(this->lfd);
	// 循环检测
	struct epoll_event evs[1024];
	int evsLen = sizeof(evs) / sizeof(struct epoll_event);
	// 循环结束的标志
	bool flag = 1;
	while (flag)
	{
		// 循环检测epoll树上文件描述符的变
		int num = epoll_wait(this->epfd, (struct epoll_event*)&evs, evsLen, -1);
		for (int i = 0; i < num; ++i)
		{
			int curfd = evs[i].data.fd;
			if (curfd == lfd)
			{
				// 此时有新的客户端连接
				std::cout << "客户端连接" << std::endl;
				int ret = acceptClient();
				if (ret == -1)
				{
					flag = 0;
					break;
				}
			}
			else
			{
				// 处理请求进行传输数据
				std::cout << "处理请求" << std::endl;
				this->cfd = curfd;
				std::cout << "连接的文件描述符" << curfd << std::endl;
				httpRSMsg();
			}
		}

	}
	close(this->lfd);
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
	ret = listen(lfd, 128);
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
	epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &ev);
	return 0;
}

int Server::acceptClient()
{
	// 通讯的文件描述符
	int cfd = accept(this->lfd, NULL, NULL);
	// 将通讯的文件描述设置为非阻塞模式
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);
	setEpollAdd(cfd);
	return 0;
}


int Server::httpRSMsg()
{
	// 先接受客户端的请求的数据
	recvMsg();
	// 向客户端发送响应数据
	sendMsg();
	return 0;
}

int Server::recvMsg()
{
	// 循环接受数据
	char buf[4096];	// 接受总数据
	char tmp[1024];	// 临时数据
	int len, total = 0;
	while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
	{
		if ((total + len) < sizeof(buf))
		{
			// buf 区数据未读满
			memcpy(buf + total, tmp, len);
		}
		total += len;
	}
	if (len == -1 && errno == EAGAIN)
	{
		// 将请求行的数据读出来
		// 在http请求中  以 \r\n 结束一行
		char* final = strstr(buf, "\r\n");
		int len = final - buf;
		// 进行截断操作
		buf[len] = '\0';
		// 解析请求行
		parseRequestLine(buf);
	}
	else if (len == 0)
	{
		// 客户端断开了连接，需要服务器断开连接, 在epoll树上删除
		std::cout << "客户端断开了连接的文件描述符" << this->cfd <<std::endl;
		disConnect();
	}
	else
	{
		// 发生接受数据错误
		perror("recv");
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
		this->fileType = getFileType(".html");
		this->fileSize = -1;
		sendHeadMsg();
		sendTrueMsg(1);
		return -1;
	}
	// 这里请求的目录
	if (S_ISDIR(st.st_mode))
	{
		// 发送头
		this->descStatus = "OK";
		this->status = 200;
		this->fileType = getFileType(".html");
		this->fileSize = -1;
		sendHeadMsg();
		// 发送数据
		sendTrueMsg(1.0);
	}
	else
	{
		// 则是文件
		this->descStatus = "OK";
		this->status = 200;
		this->fileType = getFileType(this->file);
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

int Server::disConnect()
{
	// 将cfd从epoll树上摘除
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, this->cfd, NULL);
	if (ret == -1)
	{
		std::cout << "删除失败" << std::endl;
		return -1;
	}
	// 关闭通讯的文件描述符
	close(this->cfd);
	return 0;
}
int Server::parseRequestLine(const char* buf)
{
	/*
		请求的方法  请求的资源目录 请求的数据
		
	*/
	// 请求的方法
	char method[6];
	// 请求的资源根目录
	char reqsrc[1024];
	memset(method, 0, sizeof(method));
	memset(reqsrc, 0, sizeof(reqsrc));
	// 使用 sscanf 取出对应的方法
	sscanf(buf, "%[^ ] %[^ ]", method, reqsrc);
	// 判断请求的方法是不是  get
	if (strcasecmp(method, "get") == 0)
	{
		std::cout << "请求的方法正确" << std::endl;
	}
	else
	{
		std::cout << "请求方法错误" << std::endl;
		std::cout << "method" << method << std::endl;
		return -1;
	}
	// 查询请求的资源目录
	// 解决中文问题
	Decode(reqsrc, reqsrc);
	if (strcmp(reqsrc, "/") == 0)
	{
		// 请求的是资源根目录
		this->file = "./";
	}
	else
	{
		// 这里则是访问的根目录下面的资源
		this->file = reqsrc + 1;
	}
	return 0;
}
int Server::sendHeadMsg()
{
	char buf[4096];
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
	sprintf(buf + strlen(buf), "Content-Type:%s\r\nContent-Length:%d\r\n", this->fileType, this->fileSize);
	// 空行
	sprintf(buf + strlen(buf), "\r\n");
	send(cfd, buf, strlen(buf), 0);
	return 0;
}
int Server::sendTrueMsg(int num)
{
	int fd = open(this->file, O_RDONLY | O_NONBLOCK);
	if (fd == -1)
	{
		std::cout << "打开文件失败" << std::endl;
		return -1;
	}
	char buf[4096];
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
			usleep(10);
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
	return 0;
}
int Server::sendTrueMsg(double n)
{
	// 发送目录中的内容,通过遍历
	/*
		<html>
			<head>
				<title>

				</title>
			</head>
			<body>
				<table>
					<tr>
						<td>
						</td>
					</tr>
				</table>
			</body>
		</html>
	
	*/
	char buf[4096];
	memset(buf, 0, sizeof(buf));
	// 先发送
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", this->file);
	send(this->cfd, buf, strlen(buf), 0);
	struct dirent** namelist;
	// 遍历目录
	int num = scandir(this->file, &namelist, NULL, alphasort);
	for (int i = 0; i < num; ++i)
	{
		// 获得每个文件的大小
		// 通过绝对路径获得文件的大小
		char realPath[1024];
		memset(buf, 0, sizeof(buf));
		memset(realPath, 0, sizeof(realPath));
		char* name = namelist[i]->d_name;
		sprintf(realPath, "%s/%s", this->file, name);
		struct stat st;
		stat(realPath, &st);
		if (S_ISDIR(st.st_mode))
		{
			// 还有目录进行递归, 采用超链接方式
			sprintf(buf, "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, (long long)st.st_size);
		}
		else
		{
			// 还有文件采用超链接方式
			sprintf(buf, "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, (long long)st.st_size);
		}
		send(this->cfd, buf, strlen(buf), 0);
		free(namelist[i]);
	}
	memset(buf, 0, sizeof(buf));
	// 发送剩余内容
	sprintf(buf, "</table></body></html>");
	send(this->cfd, buf, strlen(buf), 0);
	free(namelist);
	return 0;
}
