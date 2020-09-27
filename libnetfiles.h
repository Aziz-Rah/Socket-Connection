#ifndef LIBNETFILES_H
#define LIBNETFILES_H

#define PORTNO 35972
#define BUFFERSIZE 256

typedef struct structToSend{
	int fileOp; 
	int fildes; 
	size_t bytesToRead; 
	char stringBuffer[256]; 
} structToSend;

int checkIsInit;
int sockfd;

void error(char *msg);
int netserverinit(char* hostname);
int netopen(const char* pathname, int flags);
ssize_t netread(int fildes, void* buf, size_t nbyte);
ssize_t netwrite(int fildes, const void* buf, size_t nbyte);
int netclose(int fd);

#endif 