ALL: libnetfiles netfileserver

CFLAGS = gcc -g -Wall 

libnetfiles:
	$(CFLAGS) libnetfiles.c -o libnetfiles.out
	
netfileserver:
	$(CFLAGS) netfileserver.c -pthread -o netfileserver.out

clean:
	rm -rf *.out