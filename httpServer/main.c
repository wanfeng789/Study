#include "server.h"


int main(int argc, char* argv[])
{
	// argv[0] == a.out
	// argv[1] = port(�˿ں�)
	if (argc < 3)
	{
		printf("a.out port respath\n");
		exit(0);
	}
	// ������Ŀ¼ת������������ԴĿ¼
	chdir(argv[2]);

	// ����������  ���� epoll
	unsigned short port = atoi(argv[1]);
	epollRun(port);

	return 0;
}

