
#include "Server.h"

int main(int argc, char*argv[])
{
	/*
		服务器启动格式  ./Server.out port file
	*/
	if (argc < 3)
	{
		std::cout << "服务器启动格式  ./Server.out port file" << std::endl;
		exit(0);
	}
	// 更改工作目录到提供的资源根目录
	chdir(argv[2]);
	// 获取指定的端口号 
	
	Server* s = new Server(atoi(argv[1]));
	s->Serverstart();

	delete s;
	return 0;
}