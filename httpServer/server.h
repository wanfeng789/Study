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
// ��ʼ�������׽���
int initListenFd(unsigned short port);

// ���� epoll ģ��
int epollRun(unsigned short port);

// ����������
int acceptConn(int lfd, int epfd);

// ��������
int recvHttpRequest(int cfd, int epfd);
// ����������
int parseRequestLine(const char* reqpath, int cfd, int epfd);
// ������Ӧ��Ϣ��ǰ������
int sendHeadMsg(int cfd, int status, const char* descr,
				const char* type, int length);
// ���ļ����ݷ���
int sendFile(int cfd, const char* file);

// �Ͽ�����
int disConnect(int cfd, int epfd);

// ��ȡ�ļ��ĸ�ʽ
const char* getFileType(const char* filename);

// ����Ŀ¼����Ϣ
int sendDirMsg(int cfd, const char* dir);


// ת�����
int hexit(char c);
void Decode(char* to, char* from);