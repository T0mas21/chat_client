#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <regex.h>

#include "tcp_functions.h"


#define BUFFER_SIZE 1024
#define MAX_MESSAGE_SIZE 1458
#define MAX_INPUT_SIZE 1400


sem_t *sem_tcp;
pid_t main_pid_tcp;

int clientSocket_tcp = -1;
struct sockaddr_in serverAddr_tcp;






void handle_ctrl_c_tcp(int signum) 
{
    if(getpid() == main_pid_tcp)
    {
        if (clientSocket_tcp != -1) 
        {
            char message[] = "BYE\r\n";
            ssize_t bytesSent = send(clientSocket_tcp, message, strlen(message), 0);
            if (bytesSent == -1)
            {
                fprintf(stderr, "ERROR: Sending failed\n");
                sem_close(sem_tcp);
                sem_unlink("connection_handler");
                close(clientSocket_tcp);
                kill(0, SIGKILL);
            }
            close(clientSocket_tcp);
        }
        sem_close(sem_tcp);
        sem_unlink("connection_handler");
        
        kill(0, SIGKILL);
    }
    exit(signum); // Terminate the program with the received signal number
}



void myTCPconnection(char *serverAddress, __uint16_t serverPort)
{
    bool errorFlag = false;
    bool sendErrorFlag = false;
    bool byeFlag = false;
    bool repliedFlag = false;
    bool messageFlag = false;
    bool notSendFlag = false;
    bool *replied;
    bool *replyRequired;
    char buffer[BUFFER_SIZE];
    int bytesReceived;
    char toSend[MAX_MESSAGE_SIZE];
    size_t toSendLength = 0;
    char input[BUFFER_SIZE];
    Message message;
    bool *authentized; 
    bool successFlag = false;

    signal(SIGINT, handle_ctrl_c_tcp);



    // regex compile

    regex_t userNameRegex, displayNameRegex, secretTokenRegex, messageContentRegex, channelIdRegex;
    int ret;

    ret = regcomp(&userNameRegex, "^[A-Za-z0-9-]+$", REG_EXTENDED);
    if (ret) 
    {
        fprintf(stderr, "Failed to compile userName regex\n");
        exit(EXIT_FAILURE);
    }

    ret = regcomp(&displayNameRegex, "^[\x21-\x7E]+$", REG_EXTENDED);
    if (ret) 
    {
        fprintf(stderr, "Failed to compile displayName regex\n");
        regfree(&userNameRegex);
        exit(EXIT_FAILURE);
    }

    ret = regcomp(&secretTokenRegex, "^[A-Za-z0-9-]+$", REG_EXTENDED);
    if (ret) 
    {
        fprintf(stderr, "Failed to compile secretToken regex\n");
        regfree(&userNameRegex);
        regfree(&displayNameRegex);
        exit(EXIT_FAILURE);
    }

    ret = regcomp(&messageContentRegex, "^[\x20-\x7E]+$", REG_EXTENDED);
    if (ret) 
    {
        fprintf(stderr, "Failed to compile messageContent regex\n");
        regfree(&userNameRegex);
        regfree(&displayNameRegex);
        regfree(&secretTokenRegex);
        exit(EXIT_FAILURE);
    }

    ret = regcomp(&channelIdRegex, "^[A-Za-z0-9-]+$", REG_EXTENDED);
    if (ret) 
    {
        fprintf(stderr, "Failed to compile userName regex\n");
        regfree(&userNameRegex);
        regfree(&displayNameRegex);
        regfree(&secretTokenRegex);
        regfree(&messageContentRegex);
        exit(EXIT_FAILURE);
    }

    // regex compile

    // resolving

    char *IPbuffer;
    struct hostent *host_entry;

    host_entry = gethostbyname(serverAddress);
    if (host_entry == NULL) 
    {
        fprintf(stderr, "gethostbyname error\n");
        exit(EXIT_FAILURE);
    }

    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    strcpy(serverAddress, IPbuffer);
    
    // resolving


    sem_tcp = sem_open("connection_handler", O_CREAT | O_EXCL, 0666, 1);
    if (sem_tcp == SEM_FAILED) 
    {
        if (errno == EEXIST) 
        {
            sem_unlink("connection_handler");
            sem_tcp = sem_open("connection_handler", O_CREAT | O_EXCL, 0666, 1);
            if (sem_tcp == SEM_FAILED) 
            {
                perror("sem_open");
                exit(EXIT_FAILURE);
            }
        } 
        else 
        {
            perror("sem_open");
            exit(EXIT_FAILURE);
        }
    }

    

    // Create shared memory for replied
    replied = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (replied == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *replied = true; // Initialize shared variable


    // Create shared memory for replyRequired
    replyRequired = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (replyRequired == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *replyRequired = false; // Initialize shared variable

    // Create shared memory for replyRequired
    authentized = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (replyRequired == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *authentized = false; // Initialize shared variable


    clientSocket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket_tcp < 0)
    {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }


    
    memset(&serverAddr_tcp, 0, sizeof(serverAddr_tcp));
    serverAddr_tcp.sin_family = AF_INET;
    serverAddr_tcp.sin_port = htons(serverPort);

    


    if (inet_pton(AF_INET, serverAddress, &serverAddr_tcp.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        exit(EXIT_FAILURE);
    }

    if (connect(clientSocket_tcp, (struct sockaddr *)&serverAddr_tcp, sizeof(serverAddr_tcp)) < 0)
    {
        fprintf(stderr, "Connection failed\n");
        exit(EXIT_FAILURE);
    }


    main_pid_tcp = getpid();
    pid_t pid;

    // Start listening and sending
    pid = fork();

    if(pid == 0) // listening
    {
        while(1)
        {
            bytesReceived = recv(clientSocket_tcp, buffer, BUFFER_SIZE, 0);
            if (bytesReceived >= 0) 
            { 
                buffer[bytesReceived] = '\0';   
                decode_tcp(&errorFlag, &sendErrorFlag, &repliedFlag, &byeFlag, &messageFlag, &successFlag, buffer, bytesReceived);
                pid = fork();
                if(pid == 0) // handle message from server
                {
                    sem_wait(sem_tcp);

                    if(successFlag && !(*authentized))
                    {
                        *authentized = true;
                    }
                    
                    if(repliedFlag)
                    {
                        *replied = true;
                    }

                    if(sendErrorFlag)
                    {
                        strcpy(message.content, "Message does not recognized");
                        create_message_tcp("ERR", toSend, &toSendLength, &message);
                        ssize_t bytesSent = send(clientSocket_tcp, toSend, toSendLength, 0);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                            //exit(EXIT_FAILURE);
                        }
                    }

                    if(errorFlag)
                    {
                        create_message_tcp("BYE", toSend, &toSendLength, &message);
                        ssize_t bytesSent = send(clientSocket_tcp, toSend, toSendLength, 0);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                            sem_close(sem_tcp);
                            sem_unlink("connection_handler");
                            close(clientSocket_tcp);
                            kill(0, SIGKILL);
                        }

                        sem_close(sem_tcp);
                        sem_unlink("connection_handler");
                        close(clientSocket_tcp);
                        kill(0, SIGKILL);
                    }

                    if(byeFlag)
                    {
                        sem_close(sem_tcp);
                        sem_unlink("connection_handler");
                        close(clientSocket_tcp);
                        kill(0, SIGKILL);
                    }

                    sem_post(sem_tcp);
                    exit(EXIT_SUCCESS);
                }
            }
            else if (bytesReceived == 0) // Connection closed by the server
            {
                fprintf(stderr, "Connection closed by server\n");
                sem_close(sem_tcp);
                sem_unlink("connection_handler");
                close(clientSocket_tcp);
                kill(0, SIGKILL);
            }
            else
            {
                fprintf(stderr, "Connection error\n");
                sem_close(sem_tcp);
                sem_unlink("connection_handler");
                close(clientSocket_tcp);
                kill(0, SIGKILL);
            }
        }
    }
    else // sending
    {
        while(1)
        {
            sem_wait(sem_tcp);
            if((*replied && *replyRequired) || (!(*replyRequired)))
            {
                while(1)
                {
                    if(fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
                    {
                        create_message_tcp("BYE", toSend, &toSendLength, &message);
                        ssize_t bytesSent = send(clientSocket_tcp, toSend, toSendLength, 0);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                            sem_close(sem_tcp);
                            sem_unlink("connection_handler");
                            close(clientSocket_tcp);
                            kill(0, SIGKILL);
                        }

                        sem_close(sem_tcp);
                        sem_unlink("connection_handler");
                        close(clientSocket_tcp);
                        kill(0, SIGKILL);
                    }
                    //if(!set_message_tcp(&notSendFlag, input, sizeof(input), &message, &eofFlag))
                    if(!set_message_tcp(&notSendFlag, input, &message, &userNameRegex, &displayNameRegex, &secretTokenRegex, &messageContentRegex, &channelIdRegex))
                    {
                        strcpy(message.type, "MSG");
                        strcpy(message.content, input);
                    }
                    if(!notSendFlag)
                    {
                        break;
                    }
                }
                if(strcmp(message.type, "AUTH") != 0 && !(*authentized))
                {
                    fprintf(stderr, "ERROR: Authentization needed\n");
                }
                else if(strcmp(message.type, "AUTH") == 0 && *authentized)
                {
                    fprintf(stderr, "ERROR: Already authentized\n");
                }
                else
                {
                    create_message_tcp(message.type, toSend, &toSendLength, &message);
                    if(strcmp(message.type, "AUTH") == 0 || strcmp(message.type, "JOIN") == 0)
                    {
                        *replyRequired = true;
                    }
                    else
                    {
                        *replyRequired = false;
                    }

                    ssize_t bytesSent = send(clientSocket_tcp, toSend, toSendLength, 0);
                    if (bytesSent == -1)
                    {
                        fprintf(stderr, "ERROR: Sending failed\n");
                        exit(EXIT_FAILURE);
                    }

                    *replied = false;
                }
            }
            sem_post(sem_tcp);
        }
    }

    // Start listening and sending
  

    exit(0);
    // Close the socket
    //close(clientSocket_tcp);

}