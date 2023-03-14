#include <stdio.h>      // header for input and output from console : printf, perror
#include <string.h>     // strcmp
#include <sys/socket.h> // for socket related functions
#include <arpa/inet.h>  // htons
#include <netinet/in.h> // structures for addresses
#include <unistd.h>     // header for unix specic functions declarations : fork(), getpid(), getppid()
#include <stdlib.h>     // header for general fcuntions declarations: exit()
#include <sys/stat.h>   // for file size using stat()

#define PORT 9000 // port number for FTP, NOT 21?!

unsigned long ftp_port;
unsigned long data_port;
char ipAddress[256];
// for RETR
int download(int data_sock, char *filename);
// for connection to data channel
// int open_socket(int socket);
void FTPsendfile(FILE *file, int sockfd);
int list(int data_sock, int socket);

int main()
{
    // socket
    int socket_FTP = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_FTP < 0)
    {
        perror("socket:");
        exit(-1);
    }
    int login = 0;

    // setsock
    int value = 1;
    // setsockopt to reuse ports and prevent bind errors
    setsockopt(socket_FTP, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

    // server address
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY, INADDR_LOOP, inet_addr("127.0.0.1")

    // connect
    if (connect(socket_FTP, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        exit(-1);
    }

    // file variables
    FILE *file;

    // client address - finding port that is free to use for data channel
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    if (getsockname(socket_FTP, (struct sockaddr *)&client_addr, &client_size) < 0)
    {
        perror("getsockname");
        exit(-1);
    }
    ftp_port = htons(client_addr.sin_port);
    sprintf(ipAddress, "%s", inet_ntoa(client_addr.sin_addr));

    data_port = ftp_port;

    // welcome
    printf("220 Service ready for new user.\n");
    char *command; // command input var pointer

    // accept 1kb chunks of data
    char buffer[1024];
    char response[1024];
    char client_name[1024] = " ";
    size_t buffer_size = 256;
    command = (char *)malloc(buffer_size * sizeof(char));

    while (1) // client connection loop - ONLY LOCAL CALLS FOR NOW
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
        // SET LOGIN to 1 after USERAUTH FOR ALL
        if (strcmp(cmd1, "!LIST") == 0 && (login == 0)) // SET LOGIN to 1 after USERAUTH FOR ALL
        // SET LOGIN to 1 after USERAUTH FOR ALL
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
            else
            {
                printf("200 directory changed to\n");
                system("pwd");
            }
        }
        else if (strcmp(cmd1, "PWD") == 0)
        {
            if (login == 1)
            {
                strcat(buffer, cmd1);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                send(socket_FTP, buffer, strlen(buffer), 0);
                recv(socket_FTP, buffer, sizeof(buffer), 0);
                printf("%s\n", buffer);
            }
            else
            {
                printf("530 Not Logged in.\n");
            }
        }
        else if (strcmp(cmd1, "CWD") == 0)
        {

            if (login == 1)
            {
                strcpy(buffer, "CWD ");
                strcat(buffer, cmd2);
                strcat(buffer, " ");
                strcat(buffer, client_name);
                send(socket_FTP, buffer, strlen(buffer), 0);
                recv(socket_FTP, buffer, sizeof(buffer), 0);
                printf("%s\n", buffer);
            }
            else
            {
                printf("530 Not Logged in.\n");
            }
        }

        // SET LOGIN to 1 after USERAUTH
        else if (strcmp(cmd1, "PORT") == 0 && (login == 0))
        // SET LOGIN to 1 after USERAUTH
        {
            // test using cmd2 = "127,0,0,1,20,20" which is the IP + port 5140
            int p1, p2;
            p1 = data_port / 256;
            p2 = data_port % 256;
            char send_port[256] = "127,0,0,1";
            char pf[256];
            char pb[256];
            sprintf(pf, "%d", p1);
            sprintf(pb, "%d", p2);
            strcat(send_port, pf);
            strcat(send_port, ",");
            strcat(send_port, pb);
            // send port info
            int sent;
            sent = send(socket_FTP, send_port, strlen(send_port), 0);
            if (sent < 0)
            {
                perror("send socket");
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

            if (strcmp(tmp1, "loggedIn") == 0)
            {
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

        else if (strcmp(cmd1, "LIST") == 0) // refer to boilerplate in assignment3
        {
            // send the command
            strcpy(buffer, "LIST ");

            if (send(socket_FTP, buffer, strlen(buffer), 0) < 0)
            {
                perror("not sent");
            }
            recv(socket_FTP, response, sizeof(response), 0);
            // new data port number send port info to server
            data_port = data_port + 1;
            int p1, p2;
            p1 = data_port / 256;
            p2 = data_port % 256;
            char send_port[256] = "127,0,0,1";
            char pf[256];
            char pb[256];
            sprintf(pf, "%d", p1);
            sprintf(pb, "%d", p2);
            strcat(send_port, pf);
            strcat(send_port, ",");
            strcat(send_port, pb);
            // send port info
            int sent;
            sent = send(socket_FTP, send_port, strlen(send_port), 0);
            if (sent < 0)
            {
                perror("send socket");
            }
            // receive ack
            int ack;
            ack = recv(socket_FTP, buffer, sizeof(buffer), 0);

            if (ack > 0)
            {
                printf("200 PORT command successful.\n");
            }
            else
            {
                printf("port not acked");
            }

            struct sockaddr_in socket_addr, socket_transfer_addr;
            unsigned int len = sizeof(socket_addr);

            // setsock
            int value = 1;
            // setsockopt to reuse ports and prevent bind errors
            setsockopt(socket_FTP, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

            // create new port
            int newsocket = socket(AF_INET, SOCK_STREAM, 0);
            socket_addr.sin_family = AF_INET;
            socket_addr.sin_port = htons(data_port);
            socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            // bind with new port
            if (bind(newsocket, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0)
            {
                close(newsocket);
                perror("bind() error");
                return -1;
            }

            // listening
            if (listen(newsocket, 5) < 0)
            {
                close(newsocket);
                perror("listen() error");
                return -1;
            }

            unsigned int translen = sizeof(socket_transfer_addr);
            // client open data connection
            int socket_transfer = accept(newsocket, (struct sockaddr *)&socket_transfer_addr, &translen);
            if (socket_transfer < 0)
            {
                perror("accept() error");
                return -1;
            }

            // show the list
            list(socket_transfer, socket_FTP);

            // get response from server
            char response[256];
            bzero(response, sizeof(response));
            int respond = recv(socket_transfer, response, sizeof(response), 0);
            printf("%s\n", response);

            // close connection socket
            close(socket_transfer);
        }
        // STOR upload from local
        else if (strcmp(cmd1, "STOR") == 0)
        {
            // open a new port first
            if (fopen(cmd2, "r") == NULL)
            {
                perror("Can't get file");
            }
            else
            {
                // send the command
                strcpy(buffer, "STOR ");
                strcat(buffer, cmd2);

                if (send(socket_FTP, buffer, strlen(buffer), 0) < 0)
                {
                    perror("not sent");
                }
                recv(socket_FTP, response, sizeof(response), 0);
                // new data port number send port info to server
                data_port = data_port + 1;
                int p1, p2;
                p1 = data_port / 256;
                p2 = data_port % 256;
                char send_port[256] = "127,0,0,1";
                char pf[256];
                char pb[256];
                sprintf(pf, "%d", p1);
                sprintf(pb, "%d", p2);
                strcat(send_port, pf);
                strcat(send_port, ",");
                strcat(send_port, pb);
                // send port info
                int sent;
                sent = send(socket_FTP, send_port, strlen(send_port), 0);
                if (sent < 0)
                {
                    perror("send socket");
                }
                // receive ack
                int ack;
                ack = recv(socket_FTP, buffer, sizeof(buffer), 0);

                if (ack > 0)
                {
                    printf("200 PORT command successful.\n");
                }
                else
                {
                    printf("port not acked");
                }

                struct sockaddr_in socket_addr, socket_transfer_addr;
                unsigned int len = sizeof(socket_addr);

                // setsock
                int value = 1;
                // setsockopt to reuse ports and prevent bind errors
                setsockopt(socket_FTP, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

                // create new port
                int newsocket = socket(AF_INET, SOCK_STREAM, 0);
                socket_addr.sin_family = AF_INET;
                socket_addr.sin_port = htons(data_port);
                socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

                // bind with new port
                if (bind(newsocket, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0)
                {
                    close(newsocket);
                    perror("bind() error");
                    return -1;
                }

                // listening
                if (listen(newsocket, 5) < 0)
                {
                    close(newsocket);
                    perror("listen() error");
                    return -1;
                }

                unsigned int translen = sizeof(socket_transfer_addr);
                // client open data connection
                int socket_transfer = accept(newsocket, (struct sockaddr *)&socket_transfer_addr, &translen);
                if (socket_transfer < 0)
                {
                    perror("accept() error");
                    return -1;
                }
                file = fopen(cmd2, "r");
                FTPsendfile(file, socket_FTP);
            }
        }
        // RETR download to local
        else if (strcmp(cmd1, "RETR") == 0)
        {
            // send the command
            strcpy(buffer, "RETR ");
            strcat(buffer, cmd2);

            if (send(socket_FTP, buffer, strlen(buffer), 0) < 0)
            {
                perror("not sent");
            }
            recv(socket_FTP, response, sizeof(response), 0);
            // new data port number send port info to server
            data_port = data_port + 1;
            int p1, p2;
            p1 = data_port / 256;
            p2 = data_port % 256;
            char send_port[256] = "127,0,0,1";
            char pf[256];
            char pb[256];
            sprintf(pf, "%d", p1);
            sprintf(pb, "%d", p2);
            strcat(send_port, pf);
            strcat(send_port, ",");
            strcat(send_port, pb);
            // send port info
            int sent;
            sent = send(socket_FTP, send_port, strlen(send_port), 0);
            if (sent < 0)
            {
                perror("send socket");
            }
            // receive ack
            int ack;
            ack = recv(socket_FTP, buffer, sizeof(buffer), 0);

            if (ack > 0)
            {
                printf("200 PORT command successful.\n");
            }
            else
            {
                printf("port not acked");
            }

            struct sockaddr_in socket_addr, socket_transfer_addr;
            unsigned int len = sizeof(socket_addr);

            // setsock
            int value = 1;
            // setsockopt to reuse ports and prevent bind errors
            setsockopt(socket_FTP, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

            // create new port
            int newsocket = socket(AF_INET, SOCK_STREAM, 0);
            socket_addr.sin_family = AF_INET;
            socket_addr.sin_port = htons(data_port);
            socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            // bind with new port
            if (bind(newsocket, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0)
            {
                close(newsocket);
                perror("bind() error");
                return -1;
            }

            // listening
            if (listen(newsocket, 5) < 0)
            {
                close(newsocket);
                perror("listen() error");
                return -1;
            }

            unsigned int translen = sizeof(socket_transfer_addr);
            // client open data connection
            int socket_transfer = accept(newsocket, (struct sockaddr *)&socket_transfer_addr, &translen);
            if (socket_transfer < 0)
            {
                perror("accept() error");
                return -1;
            }

            // download to local
            download(socket_transfer, cmd2);

            // get response from server
            char response[256];
            bzero(response, sizeof(response));
            int respond = recv(socket_transfer, response, sizeof(response), 0);
            printf("%s\n", response);

            // close connection socket
            close(socket_transfer);
        }
    }

    close(socket_FTP);
    return 0;
}

int download(int data_sock, char *filename)
{
    char data[1024];
    int size;
    FILE *file = fopen(filename, "w");
    printf("here");

    if (!file)
    {
        perror("can't get file.");
    }
    while ((size = recv(data_sock, data, 1024, 0)) > 0)
    {
        fwrite(data, 1, size, file);
        printf("receiving");
    }

    if (size < 0)
    {
        perror("error\n");
    }

    fclose(file);
    return 0;
}

// function to send file
void FTPsendfile(FILE *file, int sockfd)
{
    char data[1024] = {0};
    while (fgets(data, 1024, file) != NULL)
    {
        if (send(sockfd, data, sizeof(data), 0) == -1)
        {
            perror("can't send file");
            exit(1);
        }
        bzero(data, 1024);
    }
}

int list(int data_sock, int socket)
{
    size_t received;
    char buf[1024];
    int here = 0;

    // receiving 150 reply
    if (recv(socket, &here, sizeof(here), 0) < 0)
    {
        perror("client: error reading message from server\n");
    }
    else
    {
        printf("%d", here);
    }

    // receiving the file containing the list
    memset(buf, 0, sizeof(buf));
    while ((received = recv(data_sock, buf, 1024, 0)) > 0)
    {
        printf("%s", buf);
        memset(buf, 0, sizeof(buf));
    }
    // if not receiving anything
    if (received < 0)
    {
        perror("error");
    }
    // for 226 reply
    if (recv(socket, &here, sizeof(here), 0) < 0)
    {
        perror("client: error reading message from server\n");
        return -1;
    }
    else
    {
        printf("%d", here);
    }

    return 0;
}