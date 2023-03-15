CC=gcc

all: server client

server: server.c
	$(CC) -o FTPserver server.c

client: client.c
	$(CC) -o FTPclient client.c

clean:
	rm -f FTPclient FTPserver