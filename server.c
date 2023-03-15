#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/stat.h>
#include <sys/select.h> // for select() and fd_set

#define PORT 9000 // NOT 21?!
#define MAX_USERS 20

unsigned long ftp_port;
char ip_addr[256];
int active_users = 0;

struct user
{
	char userName[1024];      // user name
	char userPassword[1024];  // user password
	char userDirectory[1024]; // user directory
	int loggedIn;          // flag to check login status
};

struct user users[MAX_USERS]; // max number of users


void readUsers() {
    FILE *fp = fopen("users.txt", "r");
    if (fp == NULL) {
		printf("Error opening file!\n");
		exit(1);
	}
    int i = 0;
    while (fscanf(fp, "%s %s", users[i].userName, users[i].userPassword) != EOF) {
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
	for (int i = 0; i < active_users; i++){

		if ((strcmp(users[i].userName, name) == 0) && (strcmp(users[i].userPassword, buffer) == 0)) {
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

	//socket
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	if(server_sd<0)
	{
		perror("socket:");
		exit(-1);
	}
	//setsock
	int value  = 1;
	setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY, INADDR_LOOP

	//bind
	if(bind(server_sd, (struct sockaddr*)&server_addr,sizeof(server_addr))<0)
	{
		perror("bind failed");
		exit(-1);
	}
	//listen
	if(listen(server_sd,5)<0)
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
	bzero(&cliaddr,sizeof(cliaddr));
	unsigned int len = sizeof(cliaddr);


	printf("Server is listening...\n");

	while(1) {

		ready_sockets = all_sockets;

		if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
			perror("select error");
			exit(EXIT_FAILURE);
		}

		for (int fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &ready_sockets)) {
				if (fd == server_sd) {
					// accept the connection
					int client_sd = accept(server_sd,(struct sockaddr *) &cliaddr, &len);	
					printf("[%s:%d] Connected\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));

					// add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(client_sd, &all_sockets);
				}
				else {

					// to specify client - debugging - REMOVE LATER
					printf("[%s:%d]%s", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port), " ");

					char buffer[1024]; // for storing
					char response[1024];

					bzero(response, sizeof(response));
					bzero(buffer, sizeof(buffer));

					int bytes = recv(fd, buffer, sizeof(buffer), 0);

					
					if (bytes == 0) { // connection closed 
						printf("Disconnected\n");
						close(fd);
						// removing the socket from file descriptors
						FD_CLR(fd, &all_sockets);
					}
					
					else {
						
						// splitting the response
						char *tokenArray[3];
						int index = 0;

						// Seperating by spaces
						tokenArray[index] = strtok(buffer, " \n");
						while (tokenArray[index] != NULL) {
							tokenArray[++index] = strtok(NULL, " \n");
						}

						if (strcmp(tokenArray[0], "USER") == 0) {
							authenticateUser(tokenArray[1], fd, username);
						}
						else if (strcmp(tokenArray[0], "PASS") == 0) {
							authenticatePass(tokenArray[1], fd, username);
							// login still works if password incorrect once - error checking not required in depth
						}
						else if (strcmp(tokenArray[0], "PWD") == 0) {
							// find user to get dir
							for (int i = 0; i < active_users; i++) {
								if (strcmp(users[i].userName, tokenArray[1]) == 0) {
									strcpy(response, users[i].userDirectory);
									printf("%s requested PWD\n", tokenArray[1]);
									send(fd, response, sizeof(response), 0);
								}
							}
						}
						else if (strcmp(tokenArray[0], "CWD") == 0) {
							char *command = tokenArray[1];
							char currPath[1024];

							for (int i = 0; i < active_users; i++) {
								if (strcmp(users[i].userName, tokenArray[2]) == 0) {
									// moving to user dir first
									chdir(users[i].userDirectory);

									printf("%s requested CWD\n", tokenArray[2]);

									if (chdir(command) == 0) {
										char msg[1024] = "200 directory changed to ";
										// sending path back
										getcwd(currPath, 1024);
										strcat(msg, currPath);
										send(fd, msg, sizeof(msg), 0);
										// setting new path
										strcpy(users[i].userDirectory, currPath);
									}
									else {
										perror("chdir failed");
										send(fd, "550 No such file or directory.", sizeof(response), 0);
									}
								}
							}
						}

						else if (strcmp(tokenArray[0], "LIST") == 0) {

							char *cmd1 = strtok(tokenArray[1], ":");
							char *cmd2 = strtok(NULL, " \n");

							strcpy(ip_addr, cmd1);
							ftp_port = atoi(cmd2); // FTP port string to int

							printf("%s has requested LIST\n", tokenArray[2]);

							strcpy(response, "200 PORT command successful.");
							send(fd, response, sizeof(response), 0);

							// forking the child to excecute LIST
							int pid = fork();

							if (pid == 0)
							{ 
								// create new socket for data channel
								int transfer_sock = socket(AF_INET, SOCK_STREAM, 0);
								if (transfer_sock < 0)
								{
									perror("socket");
                    				return -1;
								}

								// set up the address and port
								struct sockaddr_in client_addr;
								client_addr.sin_family = AF_INET;
								client_addr.sin_port = htons(ftp_port);
								client_addr.sin_addr.s_addr = inet_addr(ip_addr);

								// binding client to port
								struct sockaddr_in data_addr;
								data_addr.sin_family = AF_INET;
								data_addr.sin_addr.s_addr = inet_addr(ip_addr);
								data_addr.sin_port = htons(9020); // 9020,21 for data

								if (bind(transfer_sock, (struct sockaddr *)&data_addr, sizeof(struct sockaddr_in))<0) {
									perror("bind failed");
                    				continue;
								}
								// Send connection request to server:
								socklen_t addr_size = sizeof(client_addr);
								if (connect(transfer_sock, (struct sockaddr *)&client_addr, addr_size) < 0)
								{
									perror("connect");
									return -1;
								}

								strcpy(response, "150 File status okay; about to open data connection.\n");
								send(fd, response, sizeof(response), 0);
								
								for (int i = 0; i < active_users; i++)
								{
									if (strcmp(users[i].userName, tokenArray[2]) == 0)
									{
										chdir(users[i].userDirectory);
									}
								}

								// LIST data -> file -> send file -> remove file
								system("ls>list.txt");
								FILE *fp;
								fp = fopen("list.txt", "r");
								if (fp == NULL)
								{
									perror("Error opening file!\n");
									exit(1);
								}

								// getting the size of the file
								fseek(fp, 0, SEEK_END);
								int size = ftell(fp);

								// reset to beg
								fseek(fp, 0, SEEK_SET);
								
								// send the size and data in file
								char *data = malloc(size + 1);
								fread(data, size, 1, fp);
								fclose(fp);
								send(transfer_sock, &size, sizeof(int), 0);
								send(transfer_sock, data, size + 1, 0);

								// remove the files
								system("rm list.txt");
								close(transfer_sock); // close the data channel
								exit(1);
							}
							else if (pid < 0){
								perror("fork failed");
								continue;
							}
						}



						else if (strcmp(tokenArray[0], "QUIT") == 0) {
							strcpy(response, "221 Service closing control connection.");
							send(fd, response, sizeof(response), 0);
							printf("Disconnected\n");
							close(fd);
							FD_CLR(fd, &all_sockets);
						}
					}
				}
			}
		}
	}

	//close
	close(server_sd);
	return 0;
}