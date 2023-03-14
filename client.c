#include <stdio.h> // header for input and output from console : printf, perror
#include<string.h> // strcmp
#include<sys/socket.h> // for socket related functions
#include<arpa/inet.h> // htons
#include <netinet/in.h> // structures for addresses
#include<unistd.h> // header for unix specic functions declarations : fork(), getpid(), getppid()
#include<stdlib.h> // header for general fcuntions declarations: exit()
#include <sys/stat.h> // for file size using stat()

#define PORT 9000 // port number for FTP, NOT 21?!

unsigned long ftp_port;
char ipAddress[256];
void Port(char *buffer);


int main()
{
    char isLoggedIn[256];
    bzero(&isLoggedIn, sizeof(isLoggedIn));

	//socket
	int socket_FTP = socket(AF_INET,SOCK_STREAM,0);
	if(socket_FTP < 0)
	{
		perror("socket:");
		exit(-1);
	}
    int login = 0;


	//setsock
	int value  = 1;
    //setsockopt to reuse ports and prevent bind errors
	setsockopt(socket_FTP,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)


    //server address
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY, INADDR_LOOP, inet_addr("127.0.0.1")


	//connect
    if(connect(socket_FTP,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("connect");
        exit(-1);
    }
    

    // client address - finding port that is free to use for data channel
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    if (getsockname(socket_FTP, (struct sockaddr *)&client_addr, &client_size) < 0) {
        perror("getsockname");
        exit(-1);
    }
    ftp_port = htons(client_addr.sin_port);
    sprintf(ipAddress, "%s", inet_ntoa(client_addr.sin_addr));


    //welcome
	printf("220 Service ready for new user.\n");
    char *command; //command input var pointer

    //accept 1kb chunks of data
	char buffer[1024];
    char response[1024];
    char client_name[1024] = " ";
    size_t buffer_size = 256;
    command = (char *)malloc(buffer_size * sizeof(char));


	while(1) // client connection loop - ONLY LOCAL CALLS FOR NOW
	{
        // reseting the buffers and command
	    bzero(&command, sizeof(command));
        bzero(&buffer, sizeof(buffer));
        bzero(&response, sizeof(response));

        printf("ftp> ");

        getline(&command, &buffer_size, stdin);
        if (strcmp(command, "\n") == 0)
		    continue;

        // spliting command into tokens
        char *cmd1 = strtok(command, " \n"); // first token
        char *cmd2 = strtok(NULL, " \n");    // second token

        // command comparison and execution
        //SET LOGIN to 1 after USERAUTH FOR ALL
        if (strcmp(cmd1, "!LIST") == 0 && (login == 0)) //SET LOGIN to 1 after USERAUTH FOR ALL
        {
            system("ls"); // client level
        }
        else if (strcmp(cmd1, "!PWD") == 0)
        {
            system("pwd"); // client level
        }
        else if (strcmp(cmd1, "!CWD") == 0)
        {
            // chdir(cmd2); // client level

            if (chdir(cmd2) < 0)
            {
                perror("550 No such file or directory");
            }
            else{
                printf("200 directory changed to\n");
                system("pwd");
            }
        }

        //SET LOGIN to 1 after USERAUTH
        else if (strcmp(cmd1, "PORT") == 0 && (login == 0)) 
        //SET LOGIN to 1 after USERAUTH
        {
            // test using cmd2 = "127,0,0,1,20,20" which is the IP + port 5140
            Port(cmd2); // new port for data channel
        }

        else if (strcmp(cmd1, "USER") == 0)
        {
            bzero(&client_name, sizeof(client_name));
            strcpy(buffer, "USER ");
            strcat(buffer, cmd2);
            strcat(client_name, cmd2);
            // sending command to the server
            send(socket_FTP, buffer, sizeof(buffer), 0);

            recv(socket_FTP, buffer, sizeof(buffer), 0); // recieving message
            printf("%s\n", buffer);
        }

        else if (strcmp(cmd1, "PASS") == 0)
        {
            strcpy(buffer, "PASS ");
            strcat(buffer, cmd2);

            send(socket_FTP, buffer, strlen(buffer), 0); // sending the command to the network server

            recv(socket_FTP, buffer, sizeof(buffer), 0); // recieving first logged in

            char *tmp1 = strtok(buffer, " "); // first command

            char *tmp2 = strtok(NULL, "\n"); // second command

            strcpy(isLoggedIn, tmp1);

            printf("%s\n", tmp2);
        }

        else if (strcmp(cmd1, "QUIT") == 0)
        {
            memset(response, '\0', sizeof(response));
            strcpy(buffer, "QUIT");
            send(socket_FTP, buffer, sizeof(buffer), 0);
            recv(socket_FTP, response, sizeof(response), 0);
            printf("%s\n", response);
            break;
        }

        else
        {
            printf("202 Command not implemented\n");
        }



        // // boilerplate

		// if(send(socket_FTP,cmd1,strlen(cmd1),0) < 0)
		// {
		// 	perror("send");
		// 	exit(-1);
		// }
		// bzero(cmd1,sizeof(cmd1));

		// int bytes = recv(socket_FTP,cmd1,sizeof(cmd1),0);
		// printf("Server response: %s\n",cmd1);

        // // boilerplate
	}

	close(socket_FTP);
	return 0;
}


void Port(char *buffer){ // changing data port
    char *ip;
    unsigned long portNo = 0;
    char newVal[256];
    char *tokenArray[6];
    int i = 0;

    strcpy(newVal, buffer);

    char *pchs;

    pchs = strtok(newVal, " ,.-");
    while (pchs != NULL && i < 6) {
        tokenArray[i++] = pchs;
        pchs = strtok(NULL, " ,.-");
    }

    ip = (char *)malloc(256 * sizeof(char));
    sprintf(ip, "%s.%s.%s.%s", tokenArray[0], tokenArray[1], tokenArray[2], tokenArray[3]);
    portNo = atoi(tokenArray[4]) * 256 + atoi(tokenArray[5]);

    strcpy(ipAddress, ip);
    ftp_port = portNo;

    printf("%s %lu\n", ipAddress, ftp_port);

}