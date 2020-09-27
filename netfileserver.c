#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>

static const int PORTNO = 35972; //SAME as client port
static const int MAX_CONNECTIONS = 10; //max clients to have in wait queue
static const int BUFFERSIZE = 256; //max buffer to send and recieve
static const int MAXPARTS = 1000;

typedef struct structToSend{
	int fileOp; // 0 = netserverinit , 1 = netopen, 2 = netread, 3 = netwrite, 4 = close
	int fildes; // fildes for read/write
	size_t bytesToRead; //size of num bytes to read
	char stringBuffer[256]; //string
} structToSend;

typedef struct threadArgs{
	int newsockfd;
	structToSend* structBuffer;
} threadArgs;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void* clientFileHandler(void* args){
	int n = 1;
	int fileCursor = 0; //add to fildes 
	threadArgs* threadArgsPtr = (threadArgs*) args; //should I malloc this?
	structToSend* structBufferFromClient = threadArgsPtr->structBuffer; //struct inside a struct
	int newsockfd = threadArgsPtr->newsockfd;

	while(1){			
		bzero(structBufferFromClient, sizeof(structToSend));
		errno = 0; //reset errno
		n = read(newsockfd, structBufferFromClient, sizeof(structToSend)); // try to read from the client socket, n = amount of bytes sent to the client
		if (n < 1){ //error case
			error("server: ERROR cant read from socket\n");
			return NULL;
		}

		//unpack read struct from client
		int fileOp = structBufferFromClient->fileOp;
		int fildes = structBufferFromClient->fildes;
		size_t bytesToRead = structBufferFromClient->bytesToRead;
		char *stringBuffer = structBufferFromClient->stringBuffer; //sprintf(stringBuffer, "%s%c", structBufferFromClient->stringBuffer, '\0');

		structToSend* structBuffer = malloc(sizeof(structToSend));
		//find file operation (note: netserverinit shouldnt be here)

		/*
		* NETOPEN
		*/
		if (fileOp == 1){ 
			printf("-netopen-\n");
			printf("Receive from client: [%d %d %d '%s']\n", fileOp, fildes, (int)bytesToRead, stringBuffer); //debug 
			fileCursor = 0; //reset
			int file = open(stringBuffer, fildes); //open file (filename, flags) [note: using fildes for flag input]
			bzero(structBuffer, sizeof(structToSend));
			if (file < 1){ //call errno
				error("netopen: ERROR cant open file given pathname - ");
				fprintf(stderr, "%s\n", strerror(errno));
				//structBuffer: [-1, errno, 0, NULL]
				structBuffer->fileOp = -1; //neg number = error
				structBuffer->fildes = errno; //int errno
				structBuffer->bytesToRead = 0; //blank
				sprintf(structBuffer->stringBuffer, "%c",'\0'); //string errno
			} else{ //success
				//structBuffer: [1, file, 0, NULL]
				structBuffer->fileOp = 1; //redundant info
				structBuffer->fildes = file; //only important thing to output
				structBuffer->bytesToRead = 0; //blank
				sprintf(structBuffer->stringBuffer, "%c", '\0'); //blank
			}
			
		/*
		* NETREAD
		*/
		} else if (fileOp == 2){ 
			printf("-netread-\n");
			printf("Receive from client: [%d %d %d '%s']\n", fileOp, fildes, (int)bytesToRead, stringBuffer); //debug 
			bzero(stringBuffer, sizeof(BUFFERSIZE)); //clear before anything for writing
		
			int bytesRead = pread(fildes, stringBuffer, bytesToRead, (off_t)fileCursor); //read file
			bzero(structBuffer, sizeof(structToSend));
			if (bytesRead < 1){ //call errno
				error("netread: Error cant read file given file descriptor - ");
				fprintf(stderr, "%s\n", strerror(errno));
				//structBuffer: [-1, errno, 0, NULL]
				structBuffer->fileOp = -1; //neg number = error
				structBuffer->fildes = errno; //int errno
				structBuffer->bytesToRead = 0; //blank
				sprintf(structBuffer->stringBuffer, "%c",'\0'); //string errno
			} else{ //success
				fileCursor += bytesRead; //cursor follows
				//structBuffer: [2, 0, bytesRead, stringBuffer]
				structBuffer->fileOp = 2; //redundant info
				structBuffer->fildes = 0; //blank
				structBuffer->bytesToRead = bytesRead; //bytes read
				sprintf(structBuffer->stringBuffer, "%s%c", stringBuffer, '\0'); //what was read
			}

		/*
		* NETWRITE
		*/
		} else if (fileOp == 3){ 
			printf("-netwrite-\n");
			printf("Receive from client: [%d %d %d '%s']\n", fileOp, fildes, (int)bytesToRead, stringBuffer); //debug 
			//netwrite
			int bytesWrote = pwrite(fildes, stringBuffer, bytesToRead, (off_t)fileCursor); //write to file
			bzero(structBuffer, sizeof(structToSend));
			if (bytesWrote < 1){//call errno
				error("netwrite: ERROR cant write to file given file descriptor - ");
				fprintf(stderr, "%s\n", strerror(errno));
				//structBuffer: [-1, errno, 0, NULL]
				structBuffer->fileOp = -1; //neg number = error
				structBuffer->fildes = errno; //int errno
				structBuffer->bytesToRead = 0; //blank
				sprintf(structBuffer->stringBuffer, "%c",'\0'); //string errno
			} else{ //success
				fileCursor += bytesWrote; //cursor follows
				//structBuffer: [3, 0, bytesWrote, NULL]
				structBuffer->fileOp = 3; //redundant info
				structBuffer->fildes = 0; //blank
				structBuffer->bytesToRead = bytesWrote; //bytes wrote
				sprintf(structBuffer->stringBuffer, "%c", '\0'); 
			}

		/*
		* NETCLOSE
		*/
		} else if (fileOp == 4){ //CLIENT CLOSES AFTERWARDS
			printf("-netclose-\n"); 
			printf("Receive from client: [%d %d %d '%s']\n", fileOp, fildes, (int)bytesToRead, stringBuffer); //debug 
			//netclose
			int isClose = close(fildes); //close file (fildes)
			bzero(structBuffer, sizeof(structToSend));
			if (isClose < 0){ //call errno
				error("netclose: ERROR cant close file given file descriptor - ");
				fprintf(stderr, "%s\n", strerror(errno));
				//structBuffer: [-1, errno, 0, NULL]
				structBuffer->fileOp = -1; //neg number = error
				structBuffer->fildes = errno; //int errno
				structBuffer->bytesToRead = 0; //blank
				sprintf(structBuffer->stringBuffer, "%c",'\0'); //string errno
			} else{ //success
				//structBuffer: [4, isClose, 0, NULL]
				structBuffer->fileOp = 4; //redundant info
				structBuffer->fildes = isClose; //blank
				structBuffer->bytesToRead = 0; //bytes read
				sprintf(structBuffer->stringBuffer, "%c", '\0'); //what was read			
				printf("Sending to client: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer); //debug 
				
				//copied from below because not looping anymore after closes
				n = write(newsockfd, structBuffer, sizeof(structToSend)); //send back to client
				if (n < 0){
					error("netclose: ERROR telling client to disconnect\n");
					free(structBuffer);
					return NULL;
				}
				printf("Client disconnected successfully!\n");

				free(structBuffer);
				return NULL; //end connection 
			}
		} //note: should never hit else; programmer's fault

		//all ends up here (except successful close)
		printf("Sending to client: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer); //debug 
		n = write(newsockfd, structBuffer, sizeof(structToSend)); //send back to client
		if (n < 0){
			error("server: ERROR cant write to socket\n");
			free(structBuffer);
			return NULL;
		}

		free(structBuffer);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = -1;								// file descriptor for our server socket
	int newsockfd = -1;								// file descriptor for a client socket
	int clilen = -1;								// utility variable - size of clientAddressInfo below

	struct sockaddr_in serverAddressInfo;			// Super-special secret C struct that holds address info for building our server socket
	struct sockaddr_in clientAddressInfo;			// Super-special secret C struct that holds address info about our client socket
	 
	 	 
	/** If the user gave enough arguments, try to use them to get a port number and address **/
	 
	// try to build a socket .. if it doesn't work, complain and exit
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
	{
       error("ERROR opening socket\n");
       //errno = ;
       return -1;
	}
	
	/** We now have the port to build our server socket on .. time to set up the address struct **/

	// zero out the socket address info struct .. always initialize!
	bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));
	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(PORTNO);
	 
	// set a flag to indicate the type of network address we'll be using  
    serverAddressInfo.sin_family = AF_INET;
	
	// set a flag to indicate the type of network address we'll be willing to accept connections from
    serverAddressInfo.sin_addr.s_addr = INADDR_ANY;
     
	 /** We have an address struct and a socket .. time to build up the server socket **/
     
    // bind the server socket to a specific local port, so the client has a target to connect to      
    if (bind(sockfd, (struct sockaddr *) &serverAddressInfo, sizeof(serverAddressInfo)) < 0)
	{
		error("ERROR on binding\n");
		//errno = ;
		return -1;
	}

	// set up the server socket to listen for client connections
    listen(sockfd,MAX_CONNECTIONS); //"must handle up to 10"

	// determine the size of a clientAddressInfo struct
    clilen = sizeof(clientAddressInfo);
	
    /*
	* CLIENT WANTS TO CONNECT CODE
    */
    int i = 0; //count for thread joining
    threadArgs** threadArgsArr = malloc(sizeof(threadArgs)*MAXPARTS); //arr to free later
    pthread_t tidList[MAXPARTS]; //100 max parts/threads?
    while(1){ //better signal handling req
		// block until a client connects, when it does, create a client socket
    	printf("Waiting for client %d...\n", i);
    	newsockfd = accept(sockfd, (struct sockaddr*) &clientAddressInfo, &clilen); //returns new socket descriptor connecting to client to send/rec data
		printf("Connected to server!\n"); //debug
		//printf("New client connected from port %d and IP %s \n", ntohs(clientAddressInfo.sin_port, inet_ntoa(clientAddressInfo.sin_addr))); //debug
		if (newsockfd < 0){
			//connection ended
			break;
		}
		threadArgsArr[i] = (threadArgs*) malloc(sizeof(threadArgs));
		threadArgsArr[i]->newsockfd = newsockfd;
		threadArgsArr[i]->structBuffer = (structToSend*) malloc(sizeof(structToSend));
		//note: structBuffer is blank, will be filled out when server reads in thread above

		//1 Client : 1 Thread (loop)
		pthread_create(&tidList[i], NULL, clientFileHandler, threadArgsArr[i]);
		i++;
	}

	int x;
	void* useless = NULL;
	for (x = 0; x <= i; x++){ //delete the zombie children
		pthread_join(tidList[x], useless);
		free(threadArgsArr[x]->structBuffer);
		free(threadArgsArr[x]);
	}

	return 0;
}