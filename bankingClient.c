#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include "bankingClient.h"

pthread_t *serverResponseThread;
pthread_t *clientUIThread;
int running;
SOCKET _socket;
pthread_mutex_t run_lock;

int main(int argc, char *argv[])
{
	serverResponseThread = NULL;
	clientUIThread = NULL;
	if(argc < 3)
	{
		fprintf(stderr,"USE: ./bankingServer <hostname> <port>\n");
		return -1;
	}
	int port = atoi(argv[2]);
	if(port == 0)
	{
		fprintf(stderr,"Invalid port");
		return -1;
	}

	SOCKET sock = *((SOCKET *)malloc(sizeof(SOCKET)));
	for(;;)
	{
		sock = connectToServer(argv[1],atoi(argv[2]));
		if(sock != -1)
		{
			break;
		}
		delay(3);
	}
	_socket = sock;

	running = 1;
	pthread_mutex_init(&run_lock,NULL);
	serverResponseThread = spawnServerResponseThread(sock);
	clientUIThread = spawnClientUIThread(sock);

	atexit(onExit);
	signal(SIGINT,exitHandler);

	pthread_join(*serverResponseThread,NULL);
	pthread_join(*clientUIThread,NULL);


	return 0;
}
SOCKET connectToServer(char * hostname, int port)
{
	SOCKET netSocket = socket(AF_INET, SOCK_STREAM, 0);

	if(netSocket == -1)
	{
		return -1;
	}
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);

	struct hostent *hostentry;
	hostentry = gethostbyname(hostname);
	char * ipbuffer;
	ipbuffer = inet_ntoa(*((struct in_addr *) hostentry ->h_addr_list[0]));

	struct in_addr inp;
	int hostToIPSuccess = inet_aton(ipbuffer,&inp);
	if(hostToIPSuccess == 0)
	{
		fprintf(stderr,"Invalid hostname");
		return -1;
	}
	serverAddr.sin_addr.s_addr = inp.s_addr;


	int connectStatus = connect(netSocket,(struct sockaddr *) &serverAddr,sizeof(serverAddr));

	if(connectStatus == -1)
	{
		fprintf(stderr,"Connection to remote socket failed.\n%s\n",strerror(errno));
		return -1;
	}
	return netSocket;
}
pthread_t *spawnServerResponseThread(SOCKET sock)
{
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
	if(pthread_create(thread,NULL,serverResponseParserRunnable,&sock) == 0)
	{
		return thread;
	}
	else
	{
		fprintf(stderr,"Error spawning new thread.");
	}
}
pthread_t *spawnClientUIThread(SOCKET sock)
{
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
	if(pthread_create(thread,NULL,startClientUIRunnable,&sock) == 0)
	{
		return thread;
	}
	else
	{
		fprintf(stderr,"Error spawning new thread.");
	}
}
void * serverResponseParserRunnable(void *arg)
{
	signal(SIGINT,pthreadExitHandler);
	SOCKET sock = *((SOCKET *)arg);

	for(;;)
	{
		pthread_mutex_lock(&run_lock);
		if(running == 0)
		{
			pthread_mutex_unlock(&run_lock);
			break;
		}
		pthread_mutex_unlock(&run_lock);
		char response[256];
		char c;
		int remoteStatus = recv(sock,&c,1,MSG_PEEK);
		if(remoteStatus == -1)
		{
			printf("Connection to server has closed.\n%s\n",strerror(errno));
			pthread_mutex_lock(&run_lock);
			running = 0;
			pthread_mutex_unlock(&run_lock);
			break;
		}
		if(remoteStatus == 0)
		{
			delay(1);
			continue;
		}
		int bytesIn = recv(sock,&response,sizeof(response),MSG_WAITALL);
		printf("%s\n",response);
		fflush(stdout);
		response[0] = '\0';
	}
	pthread_exit(NULL);
}
void * startClientUIRunnable(void *arg)
{
	signal(SIGINT,pthreadExitHandler);
	SOCKET sock = *((SOCKET *)arg);

	printf("Welcome! ");
	fflush(stdout);
	char input[256];
	char *msg = (char *)(malloc(sizeof(char) * 256));
	for(;;)
	{
		pthread_mutex_lock(&run_lock);
		if(running == 0)
		{
			pthread_mutex_unlock(&run_lock);
			break;
		}
		pthread_mutex_unlock(&run_lock);

		printf("Enter a command.\n");

		fgets(input,256,stdin);
		if(strStartsWith(input,"create"))
		{
			if(input[6] = ' ')
			{
				strcpy(input,msg);
			}
			else
			{
				printf("USE; create <accountname>\n");
				msg[0] = '\0';
			}
		}
		else if(strStartsWith(input,"serve"))
		{
			if(input[5] = ' ')
			{
				strcpy(input,msg);
			}
			else
			{
				printf("USE; serve <accountname>\n");
				msg[0] = '\0';
			}
		}
		else if(strStartsWith(input,"deposit"))
		{
			if(input[7] = ' ')
			{
				strcpy(input,msg);
			}
			else
			{
				printf("USE; desposit <amount>\n");
				msg[0] = '\0';
			}
		}
		else if(strStartsWith(input,"withdraw"))
		{
			if(input[8] = ' ')
			{
				strcpy(input,msg);
			}
			else
			{
				printf("Use: withdraw <amount>\n");
				msg[0] = '\0';
			}
		}
		else if(strStartsWith(input,"query"))
		{
			strcpy("query",msg);
		}
		else if(strStartsWith(input,"end"))
		{
			strcpy("end",msg);
		}
		else if(strStartsWith(input,"quit"))
		{
			strcpy("quit",msg);
		}
		else
		{
			printf("Invalid command.\n");
			msg[0] = '\0';
		}
		fflush(stdout);
		if(msg[0] != '\0')
		{
			if(send(_socket,&msg,sizeof(msg),0) == -1)
			{
				fprintf(stderr,"Error sending command to server. %s\n",strerror(errno));
			}
		}
		delay(2);
	}
	free(msg);
	pthread_exit(NULL);
}
void pthreadExitHandler()
{
	pthread_mutex_lock(&run_lock);
	running = 0;
	pthread_mutex_unlock(&run_lock);
	pthread_exit(NULL);
}
void exitHandler()
{
	pthread_mutex_lock(&run_lock);
	running = 0;
	pthread_mutex_unlock(&run_lock);
	exit(1);
}
void onExit()
{
	pthread_mutex_lock(&run_lock);
	running = 0;
	pthread_mutex_unlock(&run_lock);

	pthread_mutex_destroy(&run_lock);
	free(serverResponseThread);
	free(clientUIThread);

	shutdown(_socket,2);
	close(_socket);
	printf("Goodbye.\n");
	fflush(stdout);
	return;
}
void delay(int seconds)
{
	clock_t startTime = clock();
	while(clock()/CLOCKS_PER_SEC < startTime/CLOCKS_PER_SEC + seconds){}
}
