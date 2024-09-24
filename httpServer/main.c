#include "server.h"


int main(int argc, char* argv[])
{
	// argv[0] == a.out
	// argv[1] = port(端口号)
	if (argc < 3)
	{
		printf("a.out port respath\n");
		exit(0);
	}
	// 将工作目录转到服务器的资源目录
	chdir(argv[2]);

	// 启动服务器  基于 epoll
	unsigned short port = atoi(argv[1]);
	epollRun(port);

	return 0;
}

