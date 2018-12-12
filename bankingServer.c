#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include "bankingServer.h"

SOCKET netSocket;
SOCKET test;

pthread_t accepterThread;
pthread_t clientThreads[256];
SOCKET sockets[256];

ACCOUNT accounts[256];
int numAccs;

int clientCount;
int running;

pthread_mutex_t run_lock;
pthread_mutex_t account_lock;

void delay(int seconds)
{
	clock_t startTime = clock();
	while(clock()/CLOCKS_PER_SEC < startTime/CLOCKS_PER_SEC + seconds){}
}
void end()
{
	pthread_mutex_lock(&run_lock);
	running = 1;
	pthread_mutex_unlock(&run_lock);
	exit(1);
}
void onExit()
{
	close(netSocket);
}
int main(int argc, char *argv[])
{
	signal(SIGINT,end);
	atexit(&onExit);
	running = 1;
	clientCount = 0;
	numAccs = 0;

	accepterThread = -1;
	for(int i=0;i < 256;i++)
	{
		clientThreads[i] = -1;
	}
	pthread_mutex_init(&run_lock,NULL);
	pthread_mutex_init(&account_lock,NULL);

	accepterThread = spawnAccepterThread();

	struct sigaction sa;
	struct itimerval timer;

	memset(&sa,0,sizeof(sa));
	sa.sa_handler = &printAccounts;
	sigaction(SIGVTALRM,&sa,NULL);

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 15000;

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 15000;

	setitimer(ITIMER_VIRTUAL,&timer,NULL);

	pthread_join(accepterThread,NULL);
	for(int i=0;i<clientCount;i++)
	{
		pthread_join(clientThreads[i],NULL);
	}
	return 0;
}
pthread_t spawnAccepterThread()
{
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
	if(pthread_create(thread,NULL,accepterRunnable,NULL) == 0)
	{
		return *thread;
	}
	else
	{
		fprintf(stderr,"Error spawning new thread.");
	}
}
void checkRunningHandler()
{
	pthread_mutex_lock(&run_lock);
	if(running == 0)
	{
		pthread_mutex_unlock(&run_lock);
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(&run_lock);
}
void *accepterRunnable(void *arg)
{
	netSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9484);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	bind(netSocket,(struct sockaddr *) &serverAddr,sizeof(serverAddr));

	listen(netSocket,256);

	struct sigaction sa;
	struct itimerval timer;

	memset(&sa,0,sizeof(sa));
	sa.sa_handler = &checkRunningHandler;
	sigaction(SIGVTALRM,&sa,NULL);

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 2000;

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 2000;

	setitimer(ITIMER_VIRTUAL,&timer,NULL);

	for(;;)
	{
		SOCKET clientSocket = accept(netSocket, NULL, NULL);
		if(clientSocket != -1)
		{
			if(clientCount >= 256)
			{
				close(clientSocket);
				continue;
			}
			sockets[clientCount] = clientSocket;
			clientThreads[clientCount] = spawnNewClientThread(clientCount);
			clientCount++;
		}
	}
}
pthread_t spawnNewClientThread(int sock)
{
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
	if(pthread_create(thread,NULL,newClientRunnable,&sock) == 0)
	{
		return *thread;
	}
	else
	{
		fprintf(stderr,"Error spawning new thread.");
	}
}
void *newClientRunnable(void *arg)
{
	SOCKET sock = sockets[*((int *)arg)];

	struct sigaction sa;
	struct itimerval timer;

	memset(&sa,0,sizeof(sa));
	sa.sa_handler = &checkRunningHandler;
	sigaction(SIGVTALRM,&sa,NULL);

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 2000;

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 2000;

	setitimer(ITIMER_VIRTUAL,&timer,NULL);

	printf("New client. \n");
	char *msg = (char *)malloc(sizeof(char) * 256);
	char *retmsg = (char *)malloc(sizeof(char) * 256);
	char argIn[128];
	ACCOUNT *curAcc = NULL;
	for(;;)
	{
		if(recv(sock,&msg,sizeof(msg),0) == -1)
		{
			//error reading
		}
		pthread_mutex_lock(&account_lock);
		if(strStartsWith(msg,"create"))
		{
			if(curAcc == NULL)
			{
				strcpy(argIn,&msg[7]);
				if(getAccountIndexByName(argIn) == -1)
				{
					accounts[numAccs] = *((ACCOUNT *)malloc(sizeof(ACCOUNT)));
					curAcc = &accounts[numAccs];
					numAccs++;
					curAcc->name = malloc(sizeof(char) * 128);
					curAcc->balance = 0;
					curAcc->inSession = 0;
					strcpy(retmsg,"Account created");
				}
			}
			else
			{
				strcpy(retmsg,"Still serving an account");
			}

		}
		else if(strStartsWith(msg,"serve"))
		{
			if(curAcc == NULL)
			{
				strcpy(argIn,&msg[6]);
				pthread_mutex_unlock(&account_lock);
				int index = getAccountIndexByName(argIn);
				pthread_mutex_lock(&account_lock);
				if(index != -1)
				{
					curAcc = &accounts[index];
					curAcc->inSession = 1;
				}
				else
				{
					strcpy(retmsg,"Account doesn't exist.");
				}
			}
			else
			{
				strcpy(retmsg,"Still serving an account");
			}
		}
		else if(strStartsWith(msg,"deposit"))
		{
			if(curAcc != NULL)
			{
				strcpy(argIn,&msg[8]);
				double val = 0;
				int c = sscanf(argIn,"%lf",&val);
				if(c != 1)
				{
					curAcc->balance += val;
					strcpy(retmsg,"Success");
				}
				else
				{
					strcpy(retmsg,"Failed");
				}
			}
			else
			{
				strcpy(retmsg,"No account selected");
			}

		}
		else if(strStartsWith(msg,"withdraw"))
		{
			if(curAcc != NULL)
			{
				strcpy(argIn,&msg[9]);
				double val = 0;
				int c = sscanf(argIn,"%lf",&val);
				if(c != 1)
				{
					curAcc->balance -= val;
					strcpy(retmsg,"Success");
				}
				else
				{
					strcpy(retmsg,"Failed");
				}
			}
			else
			{
				strcpy(retmsg,"No account selected");
			}
		}
		else if(strStartsWith(msg,"query"))
		{
			if(curAcc != NULL)
			{
				char bal[100];
				sprintf(bal,"%f",curAcc->balance);
				strcpy(bal,retmsg);
			}
			else
			{
				strcpy(retmsg,"No account selected");
			}
		}
		else if(strStartsWith(msg,"end"))
		{
			if(curAcc != NULL)
			{
				curAcc->inSession = 0;
				curAcc = NULL;
				strcpy(retmsg,"Account closed.");
			}
			else
			{
				strcpy(retmsg,"No account selected");
			}
		}
		else if(strStartsWith(msg,"quit"))
		{
			if(curAcc != NULL)
			{
				curAcc->inSession = 0;
				curAcc = NULL;
			}
			shutdown(sock,2);
			close(sock);
			pthread_mutex_unlock(&account_lock);
			pthread_exit(NULL);
		}
		send(sock,retmsg,sizeof(char)*256,0);
		pthread_mutex_unlock(&account_lock);
	}
}

int getAccountIndexByName(char *name)
{
	pthread_mutex_lock(&account_lock);
	for(int i=0;i<256;i++)
	{
		if(accounts[i].name != NULL)
		{
			if(strcmp(accounts[i].name,name) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}
void printAccounts()
{
	pthread_mutex_lock(&account_lock);
	printf("___ACCOUNTS___\n");
	for(int i=0;i<numAccs;i++)
	{
		if(accounts[i].name != NULL)
		{
			printf("Name: %s\t Balance: %f\t in_sesstion: %d\n",accounts[i].name,accounts[i].balance,accounts[i].inSession);
		}
	}
	pthread_mutex_unlock(&account_lock);
}
