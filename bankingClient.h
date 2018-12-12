#ifndef BANKINGCLIENT_H
#define BANKINGCLIENT_H

#include <string.h>

#define strStartsWith(string,start) (strstr((string),(start)) == (string))
typedef int SOCKET;
SOCKET connectToServer(char * hostname, int port);
void delay(int seconds);
void *serverResponseParserRunnable(void *arg);
pthread_t *spawnServerResponseThread(SOCKET sock);
pthread_t *spawnClientUIThread(SOCKET sock);
void *startClientUIRunnable(void *arg);
void exitHandler();
void onExit();
void pthreadExitHandler();

#endif
