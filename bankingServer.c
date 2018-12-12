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
pthread_t accepterThread;
pthread_t clientThreads[256];
int clientCount;
int running;

pthread_mutex_t run_lock;

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

	accepterThread = -1;
	for(int i=0;i < 256;i++)
	{
		clientThreads[i] = -1;
	}
	pthread_mutex_init(&run_lock,NULL);

	accepterThread = spawnAccepterThread();

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
			SOCKET sock = *(SOCKET *)malloc(sizeof(SOCKET));
			sock = clientSocket;
			clientThreads[clientCount] = spawnNewClientThread(&sock);
			clientCount++;
		}
	}
}
pthread_t spawnNewClientThread(SOCKET *sock)
{
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
	if(pthread_create(thread,NULL,newClientRunnable,sock) == 0)
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
	/*TODO: free sock on exit of client*/
	SOCKET sock = *((SOCKET *)arg);

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
	char msg[256];
	for(;;)
	{
		if(recv(sock,&msg,sizeof(msg),0) == -1)
		{
			//exit
		}
		else
		{
			printf("%s",msg);
		}
	}
}
