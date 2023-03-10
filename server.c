#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/stat.h>

#define PORT 9000 // NOT 21?!
#define SIZE 1024

//receive file 
void writefile(int sockfd);
void Retr(int client_sd, int sock_data, char* filename);
void STOR(int client_sd, int sock_data, char* filename);
void list(int sock_data, int client_sd);
char *response; //number response to the server

int main()
{
	//socket created
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


	struct sockaddr_in cliaddr;
    bzero(&cliaddr,sizeof(cliaddr));
    unsigned int len = sizeof(cliaddr);


	printf("Server is listening...\n");

	while(1)
	{
		//accept
		// int client_sd = accept(server_sd,0,0);

		int client_sd = accept(server_sd,(struct sockaddr *) &cliaddr, &len);
		// socket
		if (client_sd<0){
			printf("Can't find client");
			break;
		}

		printf("[%s:%d] Connected\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));	
	
		int pid = fork(); //fork a child process
		if(pid == 0)   //if it is the child process
		 {
		 	close(server_sd); //close the copy of server/master socket in child process
			//buffer to read the client input of commands
		 	char buffer[1024];
			char *command; //command input var pointer
			size_t buffer_size = 256;
    		command = (char *)malloc(buffer_size * sizeof(char));
			while(1)
			{
				bzero(buffer,sizeof(buffer));
				bzero(&command, sizeof(command));
				bzero(&response, sizeof(response));
				//receive command from client
				int bytes = recv(client_sd,buffer,sizeof(buffer),0);
				if(bytes==0)   //client has closed the connection
				{
					printf("Disconnected\n");
					close(client_sd);
					exit(1); // terminate client program
				}
				strncpy(command, buffer, &buffer_size);
        		if (strcmp(command, "\n") == 0)
		   			continue;
       			// spliting command into tokens
       		 	char *cmd1 = strtok(command, " "); // first token
        		char *cmd2 = strtok(NULL, " ");    // second token

				// command comparison and execution
				if (strcmp(cmd1, "LIST") == 0)
				{
					char buf[1024];	
					int sock_data;
					if ((sock_data = data_connect(client_sd)) < 0) {
						close(client_sd);
						exit(1); 
					}
					list(sock_data, client_sd);
					close(sock_data);
					system("ls"); // server
				}
				else if (strcmp(cmd1, "PWD") == 0)
				{
					system("pwd"); // server
				}
				else if (strcmp(cmd1, "CWD") == 0)
				{
					// chdir(cmd2); //server

					if (chdir(cmd2) < 0)
					{
						char res[]="550 No such file or directory";
						strcpy(response,res);
						int end = send(client_sd,response,strlen(response),0);
						perror("550 No such file or directory");
					}
					else{
						printf("200 directory changed to\n");
						system("pwd");
					}
				}
				// else if (strcmp(cmd1, "PORT") == 0) 
				// {
				// 	// test using cmd2 = "127,0,0,1,20,20" which is the IP + port 5140
				// 	Port(cmd2); // new port for data channel
				// }

				//when command is Quit server send response
				else if (strcmp(cmd1, "QUIT") == 0) 
				{
					char res[]="221 Service closing control connection.";
					strcpy(response,res);
					int end = send(client_sd,response,strlen(response),0);
				}
				//RETR for server to respond
				else if (strcmp(cmd1, "RETR") == 0)
				{	
					int sock_data;
					//call the client N+1 port //should get the port from client
					cliaddr.sin_port+=1;
					sock_data=connect(client_sd,(struct sockaddr *) &cliaddr, &len);
					if (sock_data < 0) {
						close(client_sd);
						exit(1); 
					}
					Retr(client_sd, sock_data, cmd2);
					close(sock_data);
				}
				//STOR for server to respond
				else if (strcmp(cmd1, "STOR") == 0)
				{	
					int sock_data;
					//call the client N+1 port //should get the port from client
					cliaddr.sin_port+=1;
					sock_data=connect(client_sd,(struct sockaddr *) &cliaddr, &len);
					if (sock_data < 0) {
						close(client_sd);
						exit(1); 
					}
					STOR(client_sd, sock_data, cmd2);
					close(sock_data);
				}
				else
				{
					printf("202 Command not implemented\n");
				}


				writefile(client_sd);
				printf("file received.");
			}
			close(client_sd);
		 }
		 else //if it is the parent process
		 {
		 	close(client_sd); //close the copy of client/secondary socket in parent process 
		 }
	}

	//close
	close(server_sd);
	return 0;
}

// void writefile(int sockfd)
// {
// 	int rec;
// 	FILE *file;
// 	char *filename;
// 	char buffer[SIZE];

// 	file=fopen(filename, "w");
// 	while(1){
// 		rec=recv(sockfd, buffer, SIZE,0);
// 		if (rec<=0){
// 			break;
// 			return;
// 		}
// 		fprintf(file, "%s", buffer);
// 		bzero(buffer, SIZE);
// 	}
// 	return;
// }

void list( int sock_data, int client_sd){

}

void STOR(int client_sd, int sock_data, char* filename){
	
}

void Retr(int client_sd, int sock_data, char* filename)
{	
	FILE* fd;
	char data[1024];
	size_t read;							
		
	fd = fopen(filename, "r");
	
	if (!fd) {	
		char res[]="550 No such file or directory";
		strcpy(response,res);
		int end = send(client_sd,response,strlen(response),0);
		
	} else {	
		char res[]="150 File status okay; about to open. data connection.";
		strcpy(response,res);
		int end1 = send(client_sd,response,strlen(response),0);
	
		while (read >= 0) {
			read = fread(data, 1, 1024, fd);

			if (read < 0) {
				printf("error fread()\n");
			}

			// send block
			if (send(sock_data, data, read, 0) < 0)
				perror("error sending file\n");

		};	

		char res2[]="226 Transfer completed.";
		strcpy(response,res2);
		int end2 = send(client_sd,response,strlen(response),0);
		
		fclose(fd);
	}
}