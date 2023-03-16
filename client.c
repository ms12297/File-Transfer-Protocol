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
char ip_addr[256];
void Port(char *buffer);


int main()
{
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
    sprintf(ip_addr, "%s", inet_ntoa(client_addr.sin_addr));


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
        if (strcmp(cmd1, "!LIST") == 0) //SET LOGIN to 1 after USERAUTH FOR ALL
        {
            system("ls"); // client level
        }
        else if (strcmp(cmd1, "!PWD") == 0)
        {
            system("pwd"); // client level
        }
        else if (strcmp(cmd1, "!CWD") == 0)
        {
            if (chdir(cmd2) < 0)
            {
                perror("550 No such file or directory");
            }
            else{
                printf("200 directory changed to\n");
                system("pwd");
            }
        }
        else if (strcmp(cmd1, "PWD") == 0) {
            if (login == 1) {
                strcat(buffer, cmd1);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                send(socket_FTP, buffer, sizeof(buffer), 0);
                recv(socket_FTP, buffer, sizeof(buffer), 0);
                printf("%s\n", buffer);
            }
            else {
                printf("530 Not Logged in.\n");
            }
            
        }
        else if (strcmp(cmd1, "CWD") == 0) {

            if (login == 1) {
                strcpy(buffer, "CWD ");
                strcat(buffer, cmd2);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                send(socket_FTP, buffer, sizeof(buffer), 0);
                recv(socket_FTP, buffer, sizeof(buffer), 0);
                printf("%s\n", buffer);
            }
            else {
                printf("530 Not Logged in.\n");
            }
            
        }
        else if (strcmp(cmd1, "LIST") == 0) {
            if (login == 1) {
                char portStr[256];

                // initiate new port in every transfer, so ++
                ftp_port++;
                sprintf(portStr, "%lu", ftp_port);

                // send LIST command to server
                strcpy(buffer, "LIST ");
                strcat(buffer, "127.0.0.1:");
                strcat(buffer, portStr);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                send(socket_FTP, buffer, sizeof(buffer), 0);

                // recieving success msg
                int bytes = recv(socket_FTP, response, sizeof(response), 0);
                printf("%s\n", response); 


                // create new socket for data channel
                int new_sock_ftp = socket(AF_INET, SOCK_STREAM, 0);
                if (new_sock_ftp < 0) {
                    perror("socket");
                    continue;
                }

                // set socket options
                int value = 1;
                setsockopt(new_sock_ftp, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
            
                // binding new sockets for the data channel
                struct sockaddr_in new_server_addr;
                bzero(&new_server_addr, sizeof(new_server_addr));
                new_server_addr.sin_family = AF_INET;
                new_server_addr.sin_port = htons(ftp_port);
                new_server_addr.sin_addr.s_addr = inet_addr(ip_addr);

                // bind new socket to port
                if(bind(new_sock_ftp, (struct sockaddr*)&new_server_addr,sizeof(new_server_addr))<0)
                {
                    perror("bind failed");
                    continue;
                }
                //listen
                if(listen(new_sock_ftp,5)<0)
                {
                    perror("listen failed");
                    close(new_sock_ftp);
                    continue;
                }

                struct sockaddr_in new_client_addr;
                bzero(&new_client_addr,sizeof(new_client_addr));
                unsigned int cl_len = sizeof(new_client_addr);

                // accept connection
                int new_client_sock = accept(new_sock_ftp, (struct sockaddr*)&new_client_addr, &cl_len);
                if (new_client_sock < 0) {
                    perror("accept");
                    continue;
                }

                // recieving transfer init msg, control so socket_FTP
                bytes = recv(socket_FTP, response, sizeof(response), 0);
                printf("%s\n", response); 


                // recieve data from server by dynamic allocated buffer
                int size;
                recv(new_client_sock, &size, sizeof(size), 0);
                char *data = malloc(size);
                recv(new_client_sock, data, size, 0);

                printf("%s\n", data);
                printf("226 Transfer complete\n");
                free(data);

                // close sockets
                close(new_client_sock);
                close(new_sock_ftp);
            }
            else {
                printf("530 Not Logged in.\n");
            }
        }

        else if (strcmp(cmd1, "STOR") == 0) {
            if (login == 1) {

                // send file
                FILE *file;
                file = fopen(cmd2, "rb");
                if (file == NULL)
                {
                    printf("550 No such file or directory.\n");
                    continue;
                }

                char portStr[256];

                // initiate new port in every transfer, so ++
                ftp_port++;
                sprintf(portStr, "%lu", ftp_port);

                // send LIST command to server
                strcpy(buffer, "STOR ");
                strcat(buffer, "127.0.0.1:");
                strcat(buffer, portStr);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                strcat(buffer, " ");
                strcat(buffer, cmd2);

                send(socket_FTP, buffer, sizeof(buffer), 0);

                // recieving success msg
                int bytes = recv(socket_FTP, response, sizeof(response), 0);
                printf("%s\n", response); 


                // create new socket for data channel
                int new_sock_ftp = socket(AF_INET, SOCK_STREAM, 0);
                if (new_sock_ftp < 0) {
                    perror("socket");
                    continue;
                }

                // set socket options
                int value = 1;
                setsockopt(new_sock_ftp, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
            
                // binding new sockets for the data channel
                struct sockaddr_in new_server_addr;
                bzero(&new_server_addr, sizeof(new_server_addr));
                new_server_addr.sin_family = AF_INET;
                new_server_addr.sin_port = htons(ftp_port);
                new_server_addr.sin_addr.s_addr = inet_addr(ip_addr);

                // bind new socket to port
                if(bind(new_sock_ftp, (struct sockaddr*)&new_server_addr,sizeof(new_server_addr))<0)
                {
                    perror("bind failed");
                    continue;
                }
                //listen
                if(listen(new_sock_ftp,5)<0)
                {
                    perror("listen failed");
                    close(new_sock_ftp);
                    continue;
                }

                struct sockaddr_in new_client_addr;
                bzero(&new_client_addr,sizeof(new_client_addr));
                unsigned int cl_len = sizeof(new_client_addr);

                // accept connection
                int new_client_sock = accept(new_sock_ftp, (struct sockaddr*)&new_client_addr, &cl_len);
                if (new_client_sock < 0) {
                    perror("accept");
                    continue;
                }

                // recieving transfer init msg, control so socket_FTP
                bytes = recv(socket_FTP, response, sizeof(response), 0);
                printf("%s\n", response); 


                // getting the size of the file
                fseek(file, 0, SEEK_END);
                int size = ftell(file);

                // reset to beg
                fseek(file, 0, SEEK_SET);

                // send the size and data in file
                char *data = malloc(size + 1);
                fread(data, size, 1, file);
                fclose(file);
                send(new_client_sock, &size, sizeof(int), 0);
                send(new_client_sock, data, size + 1, 0);
                
                // recieve success msg
                recv(new_client_sock, response, sizeof(response), 0);
                printf("%s\n", response);
                free(data);

                // close sockets
                close(new_client_sock);
                close(new_sock_ftp);

                continue;
            }
            else {
                printf("530 Not Logged in.\n");
            }
        }


        else if (strcmp(cmd1, "PORT") == 0) 
        {
            if (login == 1) {
                // test using cmd2 = "127,0,0,1,20,20" which is the IP + port 5140
                Port(cmd2);
            }
            else {
                printf("530 Not Logged in.\n");
            }
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

            if (strcmp(tmp1, "loggedIn") == 0) {
                login = 1;
            }

            char *tmp2 = strtok(NULL, "\n"); // second command

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

    strcpy(ip_addr, ip);
    ftp_port = portNo;

    printf("%s %lu\n", ip_addr, ftp_port);

}