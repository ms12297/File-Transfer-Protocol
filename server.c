#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/select.h> // for select() and fd_set

#define PORT 9000 // NOT 21?!
#define FdataPORT 9020
#define MAX_USERS 20
unsigned long data_port;
char ipAddress[256];

// receive file
void Retr(int client_sd, int sock_data, char *filename);
void STOR(int client_sd, int sock_data, char *filename);
void list(int sock_data, int client_sd);
void Port(char *buffer);
int open_socket(int socketcon, int data_port, char address[256]);
char *response; // number response to the server

int active_users = 0;

struct user
{
	char userName[1024];	  // user name
	char userPassword[1024];  // user password
	char userDirectory[1024]; // user directory
	int loggedIn;			  // flag to check login status
};

struct user users[MAX_USERS]; // max number of users

void readUsers()
{
	FILE *fp = fopen("users.txt", "r");
	if (fp == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	int i = 0;
	while (fscanf(fp, "%s %s", users[i].userName, users[i].userPassword) != EOF)
	{
		active_users++;
		i++;
	}
	fclose(fp);
}

void authenticateUser(char *buffer, int socket, char *name)
{
	// Validate user name
	for (int i = 0; i < active_users; i++)
	{
		if (strcmp(users[i].userName, buffer) == 0) // Finding the user
		{
			printf("%s is attempting login\n", buffer);
			send(socket, "331 Username OK, need password.", 99, 0);
			users[i].loggedIn = 1;
			strcpy(name, buffer);
			return;
		}
	}
	send(socket, "530 Not logged in.", 99, 0);
}

int authenticatePass(char *buffer, int socket, char *name)
{
	for (int i = 0; i < active_users; i++)
	{

		if ((strcmp(users[i].userName, name) == 0) && (strcmp(users[i].userPassword, buffer) == 0))
		{
			printf("%s has logged in\n", name);
			send(socket, "loggedIn 230 User logged in, proceed.", 99, 0);
			return 1;
		}
	}

	printf("Not logged in\n");
	send(socket, "notIn 530 Not Logged In.", 99, 0);
	return 0;
}

int main()
{
	readUsers(); // loading the data users into struct

	// initialize the users
	struct user curr_user;
	char username[1024];
	strcpy(username, "xxxxxxx");

	char path[1024];
	getcwd(path, 1024);

	for (int i = 0; i < active_users; i++)
	{
		strcpy(users[i].userDirectory, path);
		users[i].loggedIn = 0;
	}
	// socket created
	int server_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_sd < 0)
	{
		perror("socket:");
		exit(-1);
	}
	// setsock
	int value = 1;
	setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY, INADDR_LOOP

	// bind
	if (bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("bind failed");
		exit(-1);
	}
	// listen
	if (listen(server_sd, 5) < 0)
	{
		perror("listen failed");
		close(server_sd);
		exit(-1);
	}

	// 2 fd sets - back and forth
	fd_set all_sockets;
	fd_set ready_sockets;

	// zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	// add the current socket to the fd set of all sockets
	FD_SET(server_sd, &all_sockets);

	struct sockaddr_in cliaddr;
	bzero(&cliaddr, sizeof(cliaddr));
	unsigned int len = sizeof(cliaddr);

	printf("Server is listening...\n");
	while (1)
	{
		ready_sockets = all_sockets;

		if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}
		for (int fd = 0; fd < FD_SETSIZE; fd++)
		{
			if (FD_ISSET(fd, &ready_sockets))
			{
				if (fd == server_sd)
				{
					// accept the connection
					int client_sd = accept(server_sd, (struct sockaddr *)&cliaddr, &len);
					printf("[%s:%d] Connected\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

					// add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(client_sd, &all_sockets);
				}
				else
				{
					// to specify client - debugging - REMOVE LATER
					printf("[%s:%d]%s", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), " ");

					char buffer[1024]; // for storing
					char response[1024];

					bzero(response, sizeof(response));
					bzero(buffer, sizeof(buffer));

					int bytes = recv(fd, buffer, sizeof(buffer), 0);

					if (bytes == 0)
					{ // connection closed
						printf("Disconnected\n");
						close(fd);
						// removing the socket from file descriptors
						FD_CLR(fd, &all_sockets);
					}
					else
					{
						// splitting the response
						char *tokenArray[3];
						int index = 0;

						// Seperating by spaces
						tokenArray[index] = strtok(buffer, " \n");
						while (tokenArray[index] != NULL)
						{
							tokenArray[++index] = strtok(NULL, " \n");
						}

						if (strcmp(tokenArray[0], "USER") == 0)
						{
							authenticateUser(tokenArray[1], fd, username);
						}
						else if (strcmp(tokenArray[0], "PASS") == 0)
						{
							authenticatePass(tokenArray[1], fd, username);
							// login still works if password incorrect once - error checking not required in depth
						}
						else if (strcmp(tokenArray[0], "PWD") == 0)
						{
							// find user to get dir
							for (int i = 0; i < active_users; i++)
							{
								if (strcmp(users[i].userName, tokenArray[1]) == 0)
								{
									strcpy(response, users[i].userDirectory);
									send(fd, users[i].userDirectory, sizeof(users[i].userDirectory), 0);
								}
							}
						}
						if (strcmp(tokenArray[0], "CWD") == 0)
						{
							char *command = tokenArray[1];
							char currPath[1024];

							for (int i = 0; i < active_users; i++)
							{
								if (strcmp(users[i].userName, tokenArray[2]) == 0)
								{
									// moving to user dir first
									chdir(users[i].userDirectory);

									if (chdir(command) == 0)
									{
										char msg[1024] = "200 directory changed to ";
										// sending path back
										getcwd(currPath, 1024);
										strcat(msg, currPath);
										send(fd, msg, sizeof(msg), 0);
										// setting new path
										strcpy(users[i].userDirectory, currPath);
									}
									else
									{
										perror("chdir failed");
										send(fd, "550 No such file or directory.", sizeof(response), 0);
									}
								}
							}
						}
						else if (strcmp(tokenArray[0], "QUIT") == 0)
						{
							strcpy(response, "221 Service closing control connection.");
							send(fd, response, sizeof(response), 0);
							printf("Disconnected\n");
							close(fd);
							FD_CLR(fd, &all_sockets);
						}
						else if (strcmp(tokenArray[0], "LIST") == 0)
						{
							printf("LIST command received\n");
							int sock_data;
							char buf[256];
							bzero(buf, sizeof(buf));
							// call the client N+1 port //should get the port from client
							recv(fd, buf, sizeof(buf), 0); // recieving message here is port info
							printf("receiving");
							printf("%s", buf);

							// if received port and address Ack sending to client for port command
							int ack = 1;
							if (buf < 0)
							{
								perror("can't open port");
								exit(1);
							}
							else
							{
								printf("sent");
								send(fd, (char *)&ack, sizeof(ack), 0);
							}
							// get the converted ipAddress and port
							Port(buf);

							int data_sock;
							data_sock = open_socket(fd, data_port, ipAddress);
							if (data_sock < 0)
							{
								perror("Error connecting to socket");
							}

							sock_data = connect(data_sock, (struct sockaddr *)&cliaddr, len);
							if (sock_data < 0)
							{
								close(data_sock);
								exit(1);
							}
							list(sock_data, fd);
							close(sock_data);
						}
						// RETR for server to respond
						else if (strcmp(tokenArray[0], "RETR") == 0)
						{
							printf("RETR command received\n");
							int sock_data;
							char buf[256];
							bzero(buf, sizeof(buf));
							// call the client N+1 port //should get the port from client
							recv(fd, buf, sizeof(buf), 0); // recieving message here is port info
							printf("receiving");
							printf("%s", buf);

							// if received port and address Ack sending to client for port command
							int ack = 1;
							if (buf < 0)
							{
								perror("can't open port");
								exit(1);
							}
							else
							{
								printf("sent");
								send(fd, (char *)&ack, sizeof(ack), 0);
							}
							// get the converted ipAddress and port
							Port(buf);

							int data_sock;
							data_sock = open_socket(fd, data_port, ipAddress);
							if (data_sock < 0)
							{
								perror("Error connecting to socket");
							}

							sock_data = connect(data_sock, (struct sockaddr *)&cliaddr, len);
							if (sock_data < 0)
							{
								close(data_sock);
								exit(1);
							}
							Retr(data_sock, sock_data, tokenArray[1]);
							close(sock_data);
						}
						// STOR for server to respond
						else if (strcmp(tokenArray[0], "STOR") == 0)
						{
							printf("LIST command received\n");
							int sock_data;
							char buf[256];
							bzero(buf, sizeof(buf));
							// call the client N+1 port //should get the port from client
							recv(fd, buf, sizeof(buf), 0); // recieving message here is port info
							printf("receiving");
							printf("%s", buf);

							// if received port and address Ack sending to client for port command
							int ack = 1;
							if (buf < 0)
							{
								perror("can't open port");
								exit(1);
							}
							else
							{
								printf("sent");
								send(fd, (char *)&ack, sizeof(ack), 0);
							}
							// get the converted ipAddress and port
							Port(buf);

							int data_sock;
							data_sock = open_socket(fd, data_port, ipAddress);
							if (data_sock < 0)
							{
								perror("Error connecting to socket");
							}

							sock_data = connect(data_sock, (struct sockaddr *)&cliaddr, len);
							if (sock_data < 0)
							{
								close(data_sock);
								exit(1);
							}
							STOR(sock_data, fd,tokenArray[1]);
							close(sock_data);
						}
						else
						{
							strcpy(response, "202 Command not implemented.");
							send(fd, response, sizeof(response), 0);
						}
					}
				}
			}
		}
	}
	// close
	close(server_sd);
	return 0;
}

void Port(char *buffer)
{ // changing data port
	char *ip;
	unsigned long portNo = 0;
	char newVal[256];
	char *tokenArray[6];
	int i = 0;

	char newconn[2];
	printf("here");

	strcpy(newVal, buffer);

	char *pchs;

	pchs = strtok(newVal, " ,.-");
	while (pchs != NULL && i < 6)
	{
		tokenArray[i++] = pchs;
		pchs = strtok(NULL, " ,.-");
	}
	printf("here2");
	ip = (char *)malloc(256 * sizeof(char));
	sprintf(ip, "%s.%s.%s.%s", tokenArray[0], tokenArray[1], tokenArray[2], tokenArray[3]);
	portNo = atoi(tokenArray[4]) * 256 + atoi(tokenArray[5]);

	strcpy(ipAddress, ip);
	data_port = portNo;
}

int open_socket(int socketcon, int data_port, char address[256])
{
	int newsocket;
	// open the original control socket to send ack
	struct sockaddr_in socket_addr;
	unsigned int len = sizeof(socket_addr);
	// int sock_listen = connect(sock,(struct sockaddr *) &socket_addr, len);

	// create new socket
	if ((newsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() error");
		return -1;
	}
	bzero(&socket_addr, sizeof(socket_addr));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(FdataPORT);
	socket_addr.sin_addr.s_addr = INADDR_ANY;

	int value = 1;
	if (setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1)
	{
		close(newsocket);
		perror("setsockopt() error");
		return -1;
	}

	// bind
	if (bind(newsocket, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0)
	{
		close(newsocket);
		perror("bind() error");
		return -1;
	}

	struct sockaddr_in client_addr;
	bzero(&client_addr, sizeof(client_addr));
	unsigned int len2 = sizeof(client_addr);
	client_addr.sin_port = htons(data_port);
	client_addr.sin_addr.s_addr = inet_addr(ipAddress);

	// server open data connection with client
	int socket_connect = accept(newsocket, (struct sockaddr *)&client_addr, &len2);
	if (socket_connect < 0)
	{
		perror("accept() error");
		return -1;
	}

	close(newsocket);
	return socket_connect;
}

void list(int sock_data, int client_sd)
{
	char data[1024];
	size_t size;
	FILE *file;
	int res = system("ls -l > here.txt");
	printf("Retrieving list");
	file = fopen("here.txt", "r");
	if (!file)
	{
		perror("can't see list");
	}
	else
	{
		char res1[] = "150 File status okay; about to open. data connection.";
		strcpy(response, res1);
		int end = send(client_sd, response, strlen(response), 0);
	}

	fseek(file, SEEK_SET, 0);
	memset(data, 0, 1024);
	while ((size = fread(data, 1, 1024, file)) > 0)
	{
		if (send(sock_data, data, size, 0) < 0)
		{
			perror("err");
		}
		memset(data, 0, 1024);
	}
	fclose(file);
	char res2[] = "226 Transfer completed.";
	strcpy(response, res2);
	int end2 = send(client_sd, response, strlen(response), 0);
}

void STOR(int client_sd, int sock_data, char *filename)
{

	char data[1024];
	int size;
	FILE *file = fopen(filename, "w");

	while ((size = recv(client_sd, data, 1024, 0)) > 0)
	{
		fwrite(data, 1, size, file);
	}

	if (size < 0)
	{
		perror("error\n");
	}

	fclose(file);
}

void Retr(int client_sd, int sock_data, char *filename)
{
	FILE *fd;
	char data[1024];
	size_t read;

	fd = fopen(filename, "r");
	printf("hereread");

	if (!fd)
	{
		char res[] = "550 No such file or directory";
		strcpy(response, res);
		int end = send(client_sd, response, strlen(response), 0);
	}
	else
	{
		printf("here");
		char res[] = "150 File status okay; about to open. data connection.";
		strcpy(response, res);
		int end1 = send(client_sd, response, strlen(response), 0);

		while (read >= 0)
		{
			read = fread(data, 1, 1024, fd);

			if (read < 0)
			{
				printf("error fread()\n");
			}

			// send block
			if (send(sock_data, data, read, 0) < 0)
				perror("error sending file\n");
		};

		char res2[] = "226 Transfer completed.";
		strcpy(response, res2);
		int end2 = send(client_sd, response, strlen(response), 0);

		fclose(fd);
	}
}