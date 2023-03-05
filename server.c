#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/stat.h>

#define PORT 9000 // NOT 21?!

int main()
{
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


	struct sockaddr_in cliaddr;
    bzero(&cliaddr,sizeof(cliaddr));
    unsigned int len = sizeof(cliaddr);


	printf("Server is listening...\n");

	while(1)
	{
		//accept
		// int client_sd = accept(server_sd,0,0);

		int client_sd = accept(server_sd,(struct sockaddr *) &cliaddr, &len);	
		printf("[%s:%d] Connected\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));	
		// printf("%d\n", client_sd);

		// recvfrom(serverfd, buffer,256, 0,(struct sockaddr *) &cliaddr, &len);
        // printf("%s:%d \n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
		
		int pid = fork(); //fork a child process
		if(pid == 0)   //if it is the child process
		 {
		 	close(server_sd); //close the copy of server/master socket in child process
		 	char buffer[256];
			while(1)
			{
				bzero(buffer,sizeof(buffer));
				int bytes = recv(client_sd,buffer,sizeof(buffer),0);
				printf("[%s:%d] ", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
				if(bytes==0)   //client has closed the connection
				{
					printf("Disconnected\n");
					close(client_sd);
					exit(1); // terminate client program
				}
				printf("%s \n",buffer);
				int count = send(client_sd,buffer,sizeof(buffer),0);
			}
		 }
		 else //if it is the parent process
		 {
		 	close(client_sd); //close the copy of client/secondary socket in parent process 
		 }
	}


	//recv/send

	//close
	close(server_sd);
	return 0;
}