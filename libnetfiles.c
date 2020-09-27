#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "libnetfiles.h"




int checkIsInit = -1; //-1 if not ever call netserverinit, 1 if called netserverinit
int sockfd = -1; // global file descriptor for our socket
/*
typedef struct structToSend{
	int fileOp; // 0 = netserverinit , 1 = netopen, 2 = netread, 3 = netwrite, 4 = close
	int fildes; // fildes for read/write
	size_t bytesToRead; //size of num bytes to read
	char stringBuffer[256]; //string
} structToSend;
*/

void error(char *msg){
    perror(msg);
    exit(-1);
}

int netserverinit(char* hostname){ //nts: how to incorporate hostname?
	checkIsInit = 1;

	// Declare initial vars																																// utility variable - for monitoring reading/writing from/to the socket
    struct sockaddr_in serverAddressInfo;						// Super-special secret C struct that holds address info for building our socket
    struct hostent *serverIPAddress;									// Super-special secret C struct that holds info about a machine's address
	
	// look up the IP address that matches up with the name given - the name given might
	//    BE an IP address, which is fine, and store it in the 'serverIPAddress' struct
    serverIPAddress = gethostbyname(hostname);
    if (serverIPAddress == NULL)
	{
        error("netserverinit: ERROR, no such host - ");
        errno = HOST_NOT_FOUND;
        fprintf(stderr, "%s\n", strerror(errno));
		return -1;
    }
				
	// try to build a socket .. if it doesn't work, complain and exit
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //global var, already declared
    if (sockfd < 0) 
	{
        error("netserverinit: ERROR creating socket - ");
        errno = HOST_NOT_FOUND;
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
	}
	
	/** We now have the IP address and port to connect to on the server, we have to get    **/
	/**   that information into C's special address struct for connecting sockets                     **/

	// zero out the socket address info struct .. always initialize!
    bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));

	// set a flag to indicate the type of network address we'll be using 
    serverAddressInfo.sin_family = AF_INET;
	
	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(PORTNO);

	// do a raw copy of the bytes that represent the server's IP address in 
	//   the 'serverIPAddress' struct into our serverIPAddressInfo struct
    bcopy((char *)serverIPAddress->h_addr, (char *)&serverAddressInfo.sin_addr.s_addr, serverIPAddress->h_length);



	/** We now have a blank socket and a fully parameterized address info struct .. time to connect **/
	
	// try to connect to the server using our blank socket and the address info struct 
	//   if it doesn't work, complain and exit
    if (connect(sockfd,(struct sockaddr *)&serverAddressInfo,sizeof(serverAddressInfo)) < 0)  //problem here 
	{
        error("netserverinit: ERROR connecting - ");
        errno = HOST_NOT_FOUND;
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
	}	
	
    return 0; //connected to the server
}

/* errno =
EACCESS - Permission denied
EINTR -  Interrupted function call 
EISDIR - Is a directory
ENOENT -  No such file or directory (pathname dne)
EROFS - Read-only filesystem
*/
int netopen(const char* pathname, int flags){
	if (checkIsInit < 1){ // must call netserverinit before this
		error("netopen: ERROR must netserverinit first - ");
		errno = HOST_NOT_FOUND;
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}
	if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR){
		error("netopen: ERROR bad flag\n");
		//errno = ;
		return -1;
	}

	structToSend* structBuffer = malloc(sizeof(structToSend));

	//structBuffer: [1, flags, 0, pathname]
	structBuffer->fileOp = 1;
	structBuffer->fildes = flags; //O_RDONLY, O_WRONLY, O_RDWR (use fildes for flags)
	structBuffer->bytesToRead = 0; //blank
	sprintf(structBuffer->stringBuffer, "%s%c", pathname, '\0');
	printf("Sending to server: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer);

	int n = -1;
	n = write(sockfd, structBuffer, sizeof(structToSend)); //write to server
	if (n < 0){
		error("netopen: ERROR cant write to socket\n");
		//errno = ;
		free(structBuffer);
		return -1; 
	}

	////////////////////////////////////////////////////////////////////////
	bzero(structBuffer, sizeof(structToSend));

	n = read(sockfd, structBuffer, sizeof(structToSend)); //read from server
	if (n < 0){
		error("netopen: ERROR cant read from socket\n");
		//errno = ;
		free(structBuffer);
		return -1;
	}

	int fileOp = structBuffer->fileOp;
	int fildes = structBuffer->fildes;
	size_t bytesToRead = structBuffer->bytesToRead;
	char* stringBuffer = structBuffer->stringBuffer;
	printf("Server returned: [%d %d %d '%s']\n", fileOp, fildes, (int)bytesToRead, stringBuffer); //debug

	if (fileOp == -1){ //call errno
		error("netopen: ERROR cant open file - ");
		errno = fildes;
		fprintf(stderr, "%s\n", strerror(errno));
		free(structBuffer);
		return -1;
	} 

	free(structBuffer);
	return fildes; //return file desc
}

/* errno =
ETIMEDOUT - connection time out
EBADF - bad file descriptor
ECONNRESET - connection reset
*/
ssize_t netread(int fildes, void* buf, size_t nbyte){
	if (checkIsInit < 1){ // must call netserverinit before this
		error("netread: ERROR must netserverinit first - ");
		errno = HOST_NOT_FOUND;
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	structToSend* structBuffer = malloc(sizeof(structToSend));

	//gather info to struct to write
	//structBuffer: [2, fildes, nbyte, buf]
	structBuffer->fileOp = 2;
	structBuffer->fildes = fildes;
	structBuffer->bytesToRead = nbyte; 
	sprintf(structBuffer->stringBuffer, "%c", '\0');
	printf("Sending to server: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer);
	
	int n = -1;
	n = write(sockfd, structBuffer, sizeof(structToSend)); //write to server
	if (n < 0){
		error("netread: ERROR cant write to socket\n");
		//errno = ;
		free(structBuffer);
		return -1; 
	}
	////////////////////////////////////////////////////////////////////////
	bzero(structBuffer, sizeof(structToSend));

	//note: no need to bzero(structBuffer) because not using it to read
	n = read(sockfd, structBuffer, sizeof(structToSend)); //read from server to void* buf
	if (n < 0){
		error("netread: ERROR cant read from socket\n");
		//errno = ;
		free(structBuffer);
		return -1;
	}

	int fileOp = structBuffer->fileOp;
	int fildesServer = structBuffer->fildes;
	size_t bytesToRead = structBuffer->bytesToRead;
	char* stringBuffer = structBuffer->stringBuffer;
	printf("Server returned: [%d %d %d '%s']\n", fileOp, fildesServer, (int)bytesToRead, stringBuffer); //debug

	if (fileOp == -1){ //call errno
		error("netread: ERROR cant read file - ");
		errno = fildesServer;
		fprintf(stderr, "%s\n", strerror(errno));
		free(structBuffer);
		return -1;
	} 

	strcpy(buf,stringBuffer);
	free(structBuffer);
   	return n; //return bytes read
}

/* errno =
ETIMEDOUT - connection time out
EBADF - bad file descriptor
ECONNRESET - connection reset
*/
ssize_t netwrite(int fildes, const void* buf, size_t nbyte){
	if (checkIsInit < 1){ // must call netserverinit before this
		error("netwrite: ERROR must netserverinit first - ");
		errno = HOST_NOT_FOUND;
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	structToSend* structBuffer = malloc(sizeof(structToSend));

	//gather info to struct to write
	//structBuffer: [3, fildes, nbyte, buf]
	structBuffer->fileOp = 3;
	structBuffer->fildes = fildes;
	structBuffer->bytesToRead = nbyte; 
	sprintf(structBuffer->stringBuffer, "%s%c", (char*)buf, '\0');
	printf("Sending to server: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer);

	int n = -1;
	n = write(sockfd, structBuffer, sizeof(structToSend)); //write to server
	if (n < 0){
		error("netwrite: ERROR cant write to socket\n");
		//errno = ;
		free(structBuffer);
		return -1; 
	}
	////////////////////////////////////////////////////////////////////////
	bzero(structBuffer, sizeof(structToSend));

	n = read(sockfd, structBuffer, sizeof(structToSend)); //read from server
	if (n < 0){
		error("netwrite: ERROR cant read from socket\n");
		//errno = ;
		free(structBuffer);
		return -1;
	}

	int fileOp = structBuffer->fileOp;
	int fildesServer = structBuffer->fildes;
	size_t bytesToRead = structBuffer->bytesToRead; //important
	char* stringBuffer = structBuffer->stringBuffer;
	printf("Server returned: [%d %d %d '%s']\n", fileOp, fildesServer, (int)bytesToRead, stringBuffer); //debug

	if (fileOp == -1){ //call errno
		error("netwrite: ERROR cant write to file - ");
		errno = fildesServer;
		fprintf(stderr, "%s\n", strerror(errno));
		free(structBuffer);
		return -1;
	} 
	if (bytesToRead > nbyte){
		error("netwrite: ERROR bytes written greater than nbyte\n"); //for now
		//errno = ;
		free(structBuffer);
		return -1;
	}

	free(structBuffer);
   	return n; //return bytes written
}

/* errno=
EBADF - bad file descriptor
*/
int netclose(int fd){
	if (checkIsInit < 1){ // must call netserverinit before this
		error("netclose: ERROR must netserverinit first - ");
		errno = HOST_NOT_FOUND;
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	structToSend* structBuffer = malloc(sizeof(structToSend));

	//gather info to struct to write
	//structBuffer: [3, fildes, nbyte, buf]
	structBuffer->fileOp = 4;
	structBuffer->fildes = fd; //important
	structBuffer->bytesToRead = (int)0; 
	sprintf(structBuffer->stringBuffer, "%c", '\0');
	printf("Sending to server: [%d %d %d '%s']\n", structBuffer->fileOp, structBuffer->fildes, (int)structBuffer->bytesToRead, structBuffer->stringBuffer);

	int n = -1;
	n = write(sockfd, structBuffer, sizeof(structToSend)); //write to server
	if (n < 0){
		error("netclose: ERROR cant write to socket\n");
		//errno = ;
		free(structBuffer);
		return -1; 
	}
	////////////////////////////////////////////////////////////////////////
	bzero(structBuffer, sizeof(structToSend));

	n = read(sockfd, structBuffer, sizeof(structToSend)); //read from server
	if (n < 0){
		error("netclose: ERROR cant read from socket\n");
		//errno = ;
		free(structBuffer);
		return -1;
	}

	int fileOp = structBuffer->fileOp;
	int isFileClose = structBuffer->fildes; //only important things data sent (isClose) 
	size_t bytesToRead = structBuffer->bytesToRead;
	char* stringBuffer = structBuffer->stringBuffer;
	printf("Server returned: [%d %d %d '%s']\n", fileOp, isFileClose, (int)bytesToRead, stringBuffer); //debug

	if (fileOp == -1){ //call errno
		error("netclose: ERROR cant close file - ");
		errno = isFileClose;
		fprintf(stderr, "%s\n", strerror(errno));
		free(structBuffer);
		return -1;
	} 

	int isServerClose = close(sockfd);
	if (isServerClose < 0){
		error("netclose: ERROR cant close socket\n");
		//errno = ;
		free(structBuffer);
		return -1;
	} 

	printf("Disconnection Successful!\n");
	free(structBuffer);
	return 0; //ret 0 on success
}

int main(int argc, char** argv){
	char hostname[100];
	char filepath[100];

	printf("Enter hostname: ");
	scanf("%s", hostname);

	printf("Enter filepath: ");
	scanf("%s", filepath);

	printf("-netserverinit-\n");
	int connection = netserverinit(hostname);
	if(connection == -1){
		printf("Main cannot connect");
		return -1;
	}

	printf("-netopen-\n");
	int fildes = netopen(filepath, O_RDWR);
	char writeShitHere[BUFFERSIZE];
	char* readShitHere = "[WROTE HERE]"; //12

	printf("-netread-\n");
	netread(fildes, writeShitHere, 6);
	printf("..READ FROM NETREAD: '%s'\n", writeShitHere);

	printf("-netwrite-\n");
	netwrite(fildes, readShitHere, 12);

	printf("-netclose-\n");
	netclose(fildes);

	return 0;
}