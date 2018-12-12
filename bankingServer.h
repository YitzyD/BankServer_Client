#ifndef BANKINGCLIENT_H
#define BANKINGCLIENT_H

#include <string.h>

#define strStartsWith(string,start) (strstr((string),(start)) == (string))
typedef int SOCKET;
typedef struct
{
	char *name;
	double balance;
	int inSession;
}ACCOUNT;
void delay(int seconds);
void end();
void onExit();
pthread_t spawnAccepterThread();
void checkRunningHandler();
void *accepterRunnable(void *arg);
pthread_t spawnNewClientThread(int sock);
void *newClientRunnable(void *arg);
int getAccountIndexByName(char *name);
void printAccounts();

#endif
