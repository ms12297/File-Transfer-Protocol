#include <stdio.h> // header for input and output from console : printf, perror
#include <string.h> // strcmp
#include <sys/socket.h> // for socket related functions
#include <arpa/inet.h> // htons
#include <netinet/in.h> // structures for addresses
#include <unistd.h> // header for unix specic functions declarations : fork(), getpid(), getppid()
#include <stdlib.h> // header for general fcuntions declarations: exit()
#include <sys/stat.h> // for file size using stat()

#define PORT 9000 // port number for FTP, NOT 21?!
#define SIZE 1024
unsigned long ftp_port;
char ipAddress[256];
void Port(char *buffer);
//for RETR
int download(int data_sock, int socket, char* filename);
//for connection to data channel
int open_socket(int socket);
void FTPsendfile(FILE *file, int sockfd);


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
    
    // file variables 
    FILE *file;

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

        int com = send(socket_FTP, buffer, sizeof(buffer),0);
        //read messages from server
        int res=recv(socket_FTP, buffer, sizeof(buffer), 0);

        // command comparison and execution
        //SET LOGIN to 1 after USERAUTH FOR ALL
        if (strcmp(cmd1, "!LIST") == 0 && (login == 0)) //SET LOGIN to 1 after USERAUTH FOR ALL
        //SET LOGIN to 1 after USERAUTH FOR ALL
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

        else if (strcmp(cmd1, "QUIT") == 0) // refer to boilerplate in assignment3
        {
            int end = send(socket_FTP,cmd1,strlen(cmd1),0);
            close(socket_FTP);
            break;
        }
        //STOR upload from local 
        else if (strcmp(cmd1, "STOR") == 0)
        {
            char try[]="127,0,0,1,20,20";
            //open a new port first
            if (fopen(cmd2,"r")==NULL){
                perror("Can't get file");
            }
            else{
                Port(try);
                file=fopen(cmd2,"r");
                FTPsendfile(file,socket_FTP);
            }
        }
        //RETR download to local
        else if (strcmp(cmd1, "RETR") == 0)
        {
            char try2[]="127,0,0,1,20,30";
            //Port(try2);
            printf("herefunction");
            int data_sock;
            data_sock = open_socket(socket_FTP);
            if (data_sock < 0) {
				perror("Error opening socket for data connection");
			 }
            int res=recv(data_sock, buffer, sizeof(buffer), 0);
            download(data_sock, socket_FTP, cmd2);
            
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

    char newconn[2];
    printf("here");

    strcpy(newVal, buffer);

    char *pchs;

    pchs = strtok(newVal, " ,.-");
    while (pchs != NULL && i < 6) {
        tokenArray[i++] = pchs;
        pchs = strtok(NULL, " ,.-");
    }
    printf("here2");
    ip = (char *)malloc(256 * sizeof(char));
    sprintf(ip, "%s.%s.%s.%s", tokenArray[0], tokenArray[1], tokenArray[2], tokenArray[3]);
    portNo = atoi(tokenArray[4]) * 256 + atoi(tokenArray[5]);

    strcpy(ipAddress, ip);
    ftp_port = portNo;
    printf("%s,%lu", ipAddress, ftp_port);
}

int download(int data_sock, int socket, char* filename)
{
    char data[1024];
    int size;
    FILE* file = fopen(filename, "w");
    printf("here");
    
    while ((size = recv(data_sock, data, 1024, 0)) > 0) {
        fwrite(data, 1, size, file);
        printf("receiving");
    }

    if (size < 0) {
        perror("error\n");
    }

    fclose(file);
    return 0;
}

int open_socket(int socketcon)
{   
    int newsocket;
    //open the original control socket to send ack
    struct sockaddr_in socket_addr;
    unsigned int len=sizeof(socket_addr);
	//int sock_listen = connect(sock,(struct sockaddr *) &socket_addr, len);

    // create new socket
	if ((newsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket() error"); 
		return -1;
	}

    socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(ftp_port);
	socket_addr.sin_addr.s_addr = inet_addr(ipAddress) ;

    int value  = 1;
   if (setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1) {
		close(newsocket);
		perror("setsockopt() error");
		return -1; 
	}

	// bind
	if (bind(newsocket, (struct sockaddr *) &socket_addr, sizeof(socket_addr)) < 0) {
		close(newsocket);
		perror("bind() error"); 
		return -1; 
	}
   
	// begin listening
	if (listen(newsocket, 5) < 0) {
		close(newsocket);
		perror("listen() error");
		return -1;
	}              

	// Ack sending
	int ack = 1;
	if ((send(socketcon, (char*) &ack, sizeof(ack), 0)) < 0) {
		printf("client: ack write error\n");
		exit(1);
	}		

    struct sockaddr_in client_addr;
	unsigned int len2 = sizeof(client_addr);

    int socket_connect = accept(newsocket,(struct sockaddr *) &client_addr, &len2);
    if (socket_connect < 0) {
		perror("accept() error"); 
		return -1; 
	}

	close(newsocket);
	return socket_connect;
}

//function to send file
void FTPsendfile(FILE *file, int sockfd){
    char data[SIZE]={0};
    while (fgets(data, SIZE, file)!=NULL)
    {
        if (send(sockfd, data, sizeof(data),0)==-1){
            perror("can't send file");
            exit(1);
        }
        bzero(data, SIZE);
    }
}