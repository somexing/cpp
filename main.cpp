 
//
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
 

 
 

static bool bIsRunning = true;

#ifdef __GCOVER__
extern "C" void sigusr1_handler(int sig)
{
	bIsRunning = false;
}
#endif

#if defined(_WIN32) || defined(_WIN64)//Windows
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
#ifdef __GCOVER__	
	signal(SIGUSR1, sigusr1_handler);
#endif /*__GCOVER__*/

    // init socket
#if defined(_WIN32) || defined(_WIN64)//Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("init socket failed!!\n");
        return 1;
    }
#endif


    if (GetApp().start() < 0)
        return 0;

	cout <<" 2016.10 zmx 测试多路读文件发出程序和发出UDP流量程序" << endl;

	while (1) {
        char ch = getchar();
 
        if (ch == 'q' || ch == 'Q') {
            break;
        }

 
		if (ch == 't' || ch == 'T') 
			GetApp().testTCPClient ();;
		     
		if (ch == 's' || ch == 'S') 
			GetApp().testMQSender();
		if (ch == 'r' || ch == 'R')
			GetApp().testMQRecevier();

		if (ch == 'i' || ch == 'I')
			GetApp().testshowTCPInfo();

		if (ch == 'd' || ch == 'D')
			GetApp().testDelayQueue();

		if (!bIsRunning)
		{
			break;
		}
       
    }
}
