
#include "server.h"

// ��ʼ�������׽���
int initListenFd(unsigned short port)
{
	// �����������׼���
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
	{
		perror("lfd");
		return -1;
	}
	// ���ö˿ڸ���
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
	{
		perror("setsockopt");
		return -1;
	}
	// ��
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
	// ����
	ret = listen(lfd, 128);
	if (ret == -1)
	{
		perror("listen");
		return -1;
	}
	// ���ؼ����׽���
	return lfd;
}


int epollRun(unsigned short port)
{
	// ���� epoll��
	int epfd = epoll_create(10);

	int lfd = initListenFd(port);
	// ��������lfd��ӵ�epoll����
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1)
	{
		perror("epoll_ctladd");
		return -1;
	}

	// ��ʼ�������
	struct epoll_event evs[1024];
	int size_evs = sizeof(evs) / sizeof(struct epoll_event);
	int flag = 1;
	while (flag)
	{
		int num = epoll_wait(epfd, &evs, size_evs, -1);
		for (int i = 0; i < num; ++i)
		{
			int curfd = evs[i].data.fd;
			if (curfd == lfd)
			{
				// �����µ�����
				int ret = acceptConn(lfd, epfd);
				if (ret == -1)
				{
					// ���ӽ���ʧ�ܣ�ֱ���˳�
					flag = 0;
					break;
				}
			}
			else
			{
				// ��������
				recvHttpRequest(curfd, epfd);
			}
		}
	}



}

int acceptConn(int lfd, int epfd)
{
	// ��ͻ��˽�������
	int cfd = accept(lfd, NULL, NULL);
	if (cfd == -1)
	{
		perror("accept");
		return -1;
	}
	// ��ͨ�ŵ��ļ�����������Ϊ ������ ����ģʽ
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	// ��ͨ�ŵ��ļ���������ӵ�epoll����
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = cfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

	return 0;
}

int recvHttpRequest(int cfd, int epfd)
{
	// ѭ����������

	char buf[4096];	// ����������
	char tmp[1024];	// ��ʱ����
	int len, total = 0;
	while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
	{
		if ((total + len) < sizeof(buf))
		{
			// buf ������δ����
			memcpy(buf + total, tmp, len);
		}
		total += len;
	}
	if (len == -1 && errno == EAGAIN)
	{
		// �������е����ݶ�����
		// ��http������  �� \r\n ����һ��
		char* final = strstr(buf, "\r\n");
		int len = final - buf;
		// ���нضϲ���
		buf[len] = '\0';

		// ����������
		parseRequestLine(buf, cfd, epfd);

	}
	else if (len == 0)
	{
		// �ͻ��˶Ͽ������ӣ���Ҫ�������Ͽ�����, ��epoll����ɾ��
		printf("�ͻ��˶Ͽ�������\n");
		disConnect(cfd, epfd);
	}
	else
	{
		// �����������ݴ���
		perror("recv");
		return -1;
	}
	return 0;
}

int parseRequestLine(const char* reqpath, int cfd, int epfd)
{
	// �����е����������֣�����ǰ����������
	// 1.����ķ�ʽ 2.���ʷ�������λ��	

	char method[6];
	char path[1024];
	// ���������Ϣ������ȡ
	sscanf(reqpath, "%[^ ] %[^ ]", method, path);

	// 1. �ж��Ƿ�Ϊ get��ʽ ������ֱ������
	// http Э�鲻���ִ�Сд   strcasecmp() �ʺ�
	if (strcasecmp(method, "get") != 0)
	{
		printf("����ʽ����get\n");
		return -1;
	}
	// 2.�ж��������Ŀ¼�����ļ�,  --- stat()
	// ������ļ�, ���ļ����ݷ��͸��ͻ���
	// �����Ŀ¼, ���������Ŀ¼��Ŀ¼���ݷ��͸��ͻ���
	// 
	// �޸�������������
	Decode(path, path);
	char* file = NULL;
	// �ж�path���������
	if (strcmp(path, "/") == 0)
	{
		// ���ʵ��Ƿ���������Դ��Ŀ¼
		file = "./";
	}
	else
	{
		// ������Դ��Ŀ¼�������Ŀ¼, +1����ֱ�ӽ�ǰ���/ȥ��
		file = path + 1;
	}
	// ��ȡ�ļ�������

	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1)
	{
		// ��ȡ�ļ�����ʧ�ܣ�˵�������ļ�������, ��ͻ��˷���404
		sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		// ���;�����ļ�
		sendFile(cfd, "404.html");
	}
	if (S_ISDIR(st.st_mode))
	{
		// ���ʵ���Ŀ¼
		sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		sendDirMsg(cfd, file);
	}
	else 
	{
		// ���ʵ����ļ�
		// ���ļ����ݷ��͸��ͻ���
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(cfd, file);
	}

	return 0;
}

int sendHeadMsg(int cfd, int status, 
	const char* descr, const char* type, int length)
{
	// �����Ĭ��ʹ�õ� TCP ��ʽЭ��, ����һ���Է���
	 char buf[4096];
	//  ״̬�� + ��Ӧͷ + ����
	// 1. ״̬�� http/1.1 200 OK
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// 2. ��Ӧͷ content-type  content-length
	sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
	// 3. ����
	sprintf(buf + strlen(buf), "Content-Length: %d\r\n\r\n", length);

	// ��������
	send(cfd, buf, strlen(buf), 0);
	return 0;
}

int sendFile(int cfd, const char* file)
{
	// ���������ļ�
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
			// �ظ�����
			send(cfd, buf, len, 0);	// �ն������ݾͻظ���������
			// ���ڷ��͵�̫��, ���¿ͻ��˽���ʱ�ᶪʧ����
			usleep(100);
		}
		else if (len == 0)
		{
			// �Ͽ�������
			printf("�Ͽ�����\n");
			break;
		}
		else
		{
			// ���ļ�ʧ��
			perror("read");
			return -1;
		}
	}
	close(fd);
	return 0;
}

int disConnect(int cfd, int epfd)
{
	// �Ȱ�ͨѶ���ļ���������epoll����ɾ��
	epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	//�ر�ͨѶ���ļ�������
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
	// ͨ������Ŀ¼, ��ȡ����
	struct dirent **namelist;

	// ƴ�� html 
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
	sprintf(buf, "<html><head><title>%s</title></head><body><table>",
		dir);
	// alpasort ���հ�˹���������
	int num = scandir(dir, &namelist, NULL, alphasort);
	for (int i = 0; i < num; ++i)
	{
		char* name = namelist[i]->d_name;
		char rPath[1024];
		// ͨ��  stat  ����ʵ�ʵ��ļ���С, ����·��
		sprintf(rPath, "%s/%s", dir, name);
		struct stat st;
		stat(rPath, &st);
		// �����������ת����Ŀ¼
		if (S_ISDIR(st.st_mode))
		{
			// ÿһ�еĵ�һ�� �ļ���  �ڶ��� �ļ���С
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td>"
				"<td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		else
		{
			// �����������ת�����ļ�
			// ÿһ�еĵ�һ�� �ļ���  �ڶ��� �ļ���С
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td>"
				"<td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		// �������ݾͿ���ֱ�ӷ���
		send(cfd, buf, strlen(buf), 0);
		// ���buf
		memset(buf, 0, sizeof(buf));
		memset(rPath, 0, sizeof(rPath));
		// �ͷ� namelist ָ��Ŀռ��С
		free(namelist[i]);

	}
	// ����ʣ�µ�
	sprintf(buf + strlen(buf), "</table></body></html>");
	// �ٴη���
	send(cfd, buf, strlen(buf), 0);
	free(namelist);
	return 0;
}


void Decode(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from) {

		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //�����ж�from�� %20 �����ַ�

			*to = hexit(from[1]) * 16 + hexit(from[2]);//�ַ���E8�����������16���Ƶ�E8
			from += 2;                      //�ƹ��Ѿ�����������ַ�(%21ָ��ָ��1),���ʽ3��++from�����������һ���ַ�
		}
		else
			*to = *from;
	}
	*to = '\0';
}

//16������ת��Ϊ10����, return 0�������
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
