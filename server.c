#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "errutilities.h"
#include "fileutilities.h"
#include "pthrutils.h"
#include "serverops.h"
#include "tcputilities.h"


#define CONNPORT "22000"
#define CONNMAX 10
#define CONNBACKLOG 10
#define ERRMSG 1025
#define NUMTHREADS 2


int masterSocket, newSocket, clientSockets[CONNMAX];
char filenames[NFILES][FNAMESIZE];
pthread_t threads[NUMTHREADS];

void startServer();
void* handler(void* arg);
void init()
{
	srand(time(NULL));

	// Initialize all client sockets.
	int i;
	for (i=0; i<CONNMAX; i++)
		clientSockets[i] = 0;

}

int main()
{
	/* SERVER STARTUP */
	init();

	/* GENERATE FILES */
	clock_t t = clock();
	createNFiles(NFILES);
	t = clock() - t;
	fprintf(stdout, "Time taken to create files %f\n", ((double)t)/CLOCKS_PER_SEC);
	listFiles(filenames);

	/* CONFIGURE SERVER */
	startServer();

	/* ACCEPT CONNECTIONS */
	fd_set readfs;
	int i, status, tid=0, sd, activity, maxSD;
	char errmsg[ERRMSG];                                                        
	struct sockaddr_in clientaddr;     
	socklen_t addrlen;
	addrlen = sizeof(clientaddr);
	while (1)
	{
		// Clear the socket set. 
		FD_ZERO(&readfs);                                                                                                                               
        // Add master socket to set.
		FD_SET(masterSocket, &readfs);                                         
		maxSD = masterSocket;
		
		for (i=0; i<CONNMAX; i++)
		{
			sd = clientSockets[i];
			
			// If valid socket descriptor then add to ead list.
			if (sd > 0)
				FD_SET(sd, &readfs);
			// Highest file descriptor number, need it for select function.    
			if (sd > maxSD)
				maxSD = sd;
		} 
		// Wait for an activity on sockets.
		activity = select(maxSD+1, &readfs, NULL, NULL, NULL);
		if ((activity < 0) && (errno!=EINTR)) 
		{
			getError(errno, errmsg);
			fprintf(stderr, "Error on select. %s\n", errmsg);
			continue;
		}

		// Accept incoming connection.                                                          
		if (FD_ISSET(masterSocket, &readfs))  
		{
			// System call extracts the 1st connection request on queue of 
			// pending connections (CONNBACKLOG) for listening socket and 
			// creates a new connected socket, returning a file descriptor 
			// refereing to that socket.
			int commfd = accept(masterSocket, (struct sockaddr *)&clientaddr, &addrlen);
			if (commfd < 0)
			{	
				getError(errno, errmsg);
				fprintf(stderr, "Error accepting request. %s\n", errmsg);
				if((errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == ENONET || 
						errno == EHOSTUNREACH || errno == EOPNOTSUPP || errno == ENETUNREACH)) 
				{
					continue;
				}
				break;
			}
			printf("New connection, socket %d, ip: %s, port: %d\n", commfd, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			// Add new socket to array of sockets. 
			for (i=0; i<CONNMAX; i++)
			{  
				// If position is empty. 
				if (clientSockets[i] == 0)
				{ 
					clientSockets[i] = commfd;
					break; 
				}
			}
		}
		
		// Process client reuests.
		for (i=0; i<CONNMAX; i++)
		{
			sd = clientSockets[i];

			if (FD_ISSET(sd, &readfs))
			{
				params_t *params = (params_t *)malloc(sizeof(params_t));            
				params->sockfd = sd;
				status = pthread_create(&threads[tid], NULL, handler, params);  
				if (status != 0)
				{ 
					getError(status, errmsg);       
					fprintf(stderr, "Error creating thread. %s\n", errmsg);     
					break;   
				}
				tid = (tid+1) % NUMTHREADS;
				clientSockets[i] = 0; 
			}
		}
	}

	shutdown(masterSocket, SHUT_RDWR);
	close(masterSocket);

	exit(0);
}



void startServer() 
{
	int status, opt=1;
	char errmsg[ERRMSG];
	char port[6];
	strcpy(port, CONNPORT);
	struct addrinfo hints, *res, *p;
	
	// getaddrinfo for host.
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	status = getaddrinfo(NULL, port, &hints, &res);
	if (status != 0)
	{
		getError(errno, errmsg);
		fprintf(stderr, "Error in getaddrinfo. %s\n", errmsg);
		exit(1);
	}

	// Bind a socket.
	for (p=res; p!=NULL; p=p->ai_next)
	{
		bzero(errmsg, sizeof(errmsg));
		// Creates and endpoint for communication and return a file descriptor.     
		// The first argument specifies the communication domain; selects the 
		// protocol family to be used.                                           
		masterSocket = socket(p->ai_family, p->ai_socktype, 0);                           
		if (masterSocket <= 0)                                                          
		{                                                   
			getError(errno, errmsg);
			fprintf(stderr, "Error creating a socket. %s\n", errmsg);      
			continue;                        
		}
		// Allow multiple connections.
		status = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
		if (status < 0)
		{
			getError(errno, errmsg);
			fprintf(stderr, "Error setting sock opts. %s\n", errmsg);           
			break;
		}
		// Bind a name to a socket. Assign the address specified by servaddr to 
		// socket refered by file descriptor masterSocket.                                                    
		status = bind(masterSocket, p->ai_addr, p->ai_addrlen);  
		if (status == 0)
			break;
		printf("Listening on port %s\n", port);
	}
	if (p==NULL)
	{
		getError(errno, errmsg);
		fprintf(stderr, "Error creating or binding address to socket. %s\n", errmsg);
		exit(1);             
	}

	// Free the dynamically allocated linked list res.
	freeaddrinfo(res);

	// Listen for connections on a socket.
	// Mark the socket lsiten_fd as a passive socket (accept incoming 
	// connection requests).                                                           
	status = listen(masterSocket, CONNBACKLOG);
	if (status < 0)
	{
		getError(errno, errmsg);
		fprintf(stderr, "Error while marking masterSocket as passive socket. %s\n", errmsg);
		exit(-1);
	}
}


void* handler(void* arg)
{
	int i;
	params_t *params = (params_t *)arg;

	printf("Sending files over to %d\n", params->sockfd);    
	for (i=0; i<NFILES; i++)  
	{                                                               
		//if (i%100==0)  
		//	printf("Sending file %d\n", i);                         
		serverSendFile(params->sockfd, filenames[i]);                           
	}                                                               
	printf("Done sending files over to %d\n", params->sockfd); 
	closeSocket(params->sockfd);
	return NULL;
}
