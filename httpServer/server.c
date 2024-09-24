
#include "server.h"

// 初始化监听套接字
int initListenFd(unsigned short port)
{
	// 创建监听的套件字
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
	{
		perror("lfd");
		return -1;
	}
	// 设置端口复用
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
	{
		perror("setsockopt");
		return -1;
	}
	// 绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1)
	{
		perror("bind");
		return -1;
	}
	// 监听
	ret = listen(lfd, 128);
	if (ret == -1)
	{
		perror("listen");
		return -1;
	}
	// 返回监听套接字
	return lfd;
}


int epollRun(unsigned short port)
{
	// 创建 epoll树
	int epfd = epoll_create(10);

	int lfd = initListenFd(port);
	// 将监听的lfd添加到epoll树上
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1)
	{
		perror("epoll_ctladd");
		return -1;
	}

	// 开始持续检测
	struct epoll_event evs[1024];
	int size_evs = sizeof(evs) / sizeof(struct epoll_event);
	int flag = 1;
	while (flag)
	{
		int num = epoll_wait(epfd, &evs, size_evs, 1000);
		for (int i = 0; i < num; ++i)
		{
			int curfd = evs[i].data.fd;
			if (curfd == lfd)
			{
				// 建立新的连接
				int ret = acceptConn(lfd, epfd);
				if (ret == -1)
				{
					// 连接建立失败，直接退出
					flag = 0;
					break;
				}
			}
			else
			{
				// 接受数据
				recvHttpRequest(curfd, epfd);
			}
		}
	}



}

int acceptConn(int lfd, int epfd)
{
	// 与客户端建立连接
	int cfd = accept(lfd, NULL, NULL);
	if (cfd == -1)
	{
		perror("accept");
		return -1;
	}
	// 将通信的文件描述符设置为 非阻塞 边沿模式
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	// 将通信的文件描述符添加到epoll树上
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = cfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

	return 0;
}

int recvHttpRequest(int cfd, int epfd)
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
			parseRequestLine(buf, cfd, epfd);
		}

		else if (len == 0)
		{
			// 客户端断开了连接，需要服务器断开连接, 在epoll树上删除
			printf("客户端断开了连接\n");
			disConnect(cfd, epfd);
		}
		else
		{
			// 发生接受数据错误
			perror("recv");
			return -1;
		}
	
	
	return 0;
}

int parseRequestLine(const char* reqpath, int cfd, int epfd)
{
	// 请求行的数据三部分，其中前两部分有用
	// 1.请求的方式 2.访问服务器的位置	

	char method[6];
	char path[1024];
	// 将请求的信息进行提取
	sscanf(reqpath, "%[^ ] %[^ ]", method, path);

	// 1. 判断是否为 get方式 不是则直接跳过
	// http 协议不区分大小写   strcasecmp() 适合
	if (strcasecmp(method, "get") != 0)
	{
		printf("请求方式不是get\n");
		return -1;
	}
	// 2.判断是请求的目录还是文件,  --- stat()
	// 如果是文件, 则将文件内容发送给客户端
	// 如果是目录, 则遍历整个目录将目录内容发送给客户端
	// 
	// 修改中文名字问题
	Decode(path, path);
	char* file = NULL;
	// 判断path里面的类型
	if (strcmp(path, "/") == 0)
	{
		// 访问的是服务器的资源根目录
		file = "./";
	}
	else
	{
		// 则是资源根目录下面的子目录, +1就是直接将前面的/去掉
		file = path + 1;
	}
	// 获取文件的属性

	struct stat st;
	
	int ret = stat(file, &st);
	if (ret == -1)
	{
		// 获取文件属性失败，说明访问文件不存在, 向客户端发送404
		sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		// 发送具体的文件
		sendFile(cfd, "404.html");
	}
	if (S_ISDIR(st.st_mode))
	{
		// 访问的是目录
		sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		sendDirMsg(cfd, file);
	}
	else 
	{
		// 访问的是文件
		// 将文件内容发送给客户端
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(cfd, file);
	}

	return 0;
}

int sendHeadMsg(int cfd, int status, 
	const char* descr, const char* type, int length)
{
	// 传输层默认使用的 TCP 流式协议, 无需一次性发送
	 char buf[4096];
	//  状态行 + 响应头 + 空行
	// 1. 状态行 http/1.1 200 OK
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// 2. 响应头 content-type  content-length
	sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
	// 3. 空行
	sprintf(buf + strlen(buf), "Content-Length: %d\r\n\r\n", length);

	// 发送数据
	send(cfd, buf, strlen(buf), 0);
	return 0;
}

int sendFile(int cfd, const char* file)
{
	// 读出本地文件
	int fd = open(file, O_RDONLY);
	if (fd == -1)
	{
		perror("open");
		return -1;
	}
	while (1)
	{
		char buf[1024] = { 0 };
		int len = read(fd, buf, sizeof(buf));
		if (len > 0)
		{
			// 回复数据
			send(cfd, buf, len, 0);	// 收多少数据就回复多少数据
			// 由于发送的太快, 导致客户端解析时会丢失数据
			usleep(100);
		}
		else if (len == 0)
		{
			break;
		}
		else
		{
			// 读文件失败
			perror("read");
			return -1;
		}
	}
	close(fd);
	return 0;
}

int disConnect(int cfd, int epfd)
{
	// 先把通讯的文件描述符在epoll树上删除
	epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	//关闭通讯的文件描述符
	close(cfd);

	return 0;
}

const char* getFileType(const char* filename)
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

int sendDirMsg(int cfd, const char* dir)
{
	// 通过遍历目录, 获取内容
	struct dirent **namelist;

	// 拼接 html 
	/*
	* <html>
	*	<head>
	*		<title>xxx</title>
	*	</head>
	*	<body>
	*		<table>
	*			<tr>
	*			<td>xxx</td>
	*			</tr>
	*		</talbe>
	*	</body>
	* </html>
	*/
	char buf[4096];
	sprintf(buf, "<html><head><title>%s</title></head><body><table align=\"left\">",
		dir);
	// alpasort 按照阿斯克码表排序
	int num = scandir(dir, &namelist, NULL, alphasort);
	for (int i = 0; i < num; ++i)
	{
		char* name = namelist[i]->d_name;
		char rPath[1024];
		// 通过  stat  计算实际的文件大小, 绝对路径
		sprintf(rPath, "%s/%s", dir, name);
		struct stat st;
		stat(rPath, &st);
		// 如果超链接跳转的是目录
		if (S_ISDIR(st.st_mode))
		{
			// 每一行的第一列 文件名  第二列 文件大小
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td>"
				"<td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		else
		{
			// 如果超链接跳转的是文件
			// 每一行的第一列 文件名  第二列 文件大小
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td>"
				"<td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		// 接受数据就可以直接发送
		send(cfd, buf, strlen(buf), 0);
		// 清空buf
		memset(buf, 0, sizeof(buf));
		memset(rPath, 0, sizeof(rPath));
		// 释放 namelist 指向的空间大小
		free(namelist[i]);

	}
	// 补充剩下的
	sprintf(buf + strlen(buf), "</table></body></html>");
	// 再次发送
	send(cfd, buf, strlen(buf), 0);
	free(namelist);
	return 0;
}


void Decode(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from) {

		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //依次判断from中 %20 三个字符

			*to = hexit(from[1]) * 16 + hexit(from[2]);//字符串E8变成了真正的16进制的E8
			from += 2;                      //移过已经处理的两个字符(%21指针指向1),表达式3的++from还会再向后移一个字符
		}
		else
			*to = *from;
	}
	*to = '\0';
}

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
