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

#include "udp_functions.h"


#define MAX_INPUT_SIZE 1400









sem_t *sem_udp;
pid_t main_pid_udp;
int clientSocket_udp = -1;
struct sockaddr_in serverAddr;
socklen_t serverAddrLen = 0;
uint16_t *messageID;









void handle_ctrl_c(int signum) 
{
    if(getpid() == main_pid_udp)
    {
        if (clientSocket_udp != -1) 
        {
            u_int8_t message[3];
            message[0] = 0xFF;
            memcpy(message + 1, messageID, sizeof(*messageID));
            ssize_t bytesSent = sendto(clientSocket_udp, message, sizeof(message), 0, (struct sockaddr *)&serverAddr, serverAddrLen);
            if (bytesSent == -1)
            {
                fprintf(stderr, "ERROR: Sending failed\n");
                sem_close(sem_udp);
                sem_unlink("connection_handler");
                close(clientSocket_udp);
                kill(0, SIGKILL);
            }
            close(clientSocket_udp);
        }
        sem_close(sem_udp);
        sem_unlink("connection_handler");
        
        kill(0, SIGKILL);
    }
    exit(signum); // Terminate the program with the received signal number
}




void myUDPconnection(char *serverAddress, __uint16_t serverPort, __uint16_t udpTimeout, __uint8_t udpMaxRetransmissions)
{
    uint8_t toSend[MAX_MESSAGE_SIZE];
    size_t toSendLength = 0;
    uint8_t buffer[BUFFER_SIZE];
    int bytesReceived;
    bool errorFlag = false;
    bool sendErrorFlag = false;
    bool byeFlag = false;
    bool *replied;
    bool *confirmed;
    bool *replyRequired;
    bool confirmFlag = false;
    bool repliedFlag = false;
    bool messageFlag = false;
    bool notSendFlag = false;
    char input[BUFFER_SIZE];
    Message message;
    uint8_t confirmMessage[3];
    bool *authentized; 
    bool successFlag = false;
    
    uint8_t *toSend_shared;
    size_t *toSendLength_shared;

    signal(SIGINT, handle_ctrl_c);

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

    sem_udp = sem_open("connection_handler", O_CREAT | O_EXCL, 0666, 1);
    if (sem_udp == SEM_FAILED) 
    {
        if (errno == EEXIST) 
        {
            sem_unlink("connection_handler");
            sem_udp = sem_open("connection_handler", O_CREAT | O_EXCL, 0666, 1);
            if (sem_udp == SEM_FAILED) 
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

    // Create shared memory for confirmed
    confirmed = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (confirmed == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *confirmed = true; // Initialize shared variable

    // Create shared memory for messageID
    messageID = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (messageID == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *messageID = 0; // Initialize shared variable

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


    // Create shared memory for toSend array
    toSend_shared = mmap(NULL, MAX_MESSAGE_SIZE * sizeof(uint8_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (toSend_shared == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Create shared memory for toSendLength variable
    toSendLength_shared = mmap(NULL, sizeof(size_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (toSendLength_shared == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    *toSendLength_shared = 0;


    clientSocket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if(clientSocket_udp < 0)
    {
        fprintf(stderr, "ERROR: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverAddress);
    serverAddr.sin_port = htons(serverPort);

    

    serverAddrLen = sizeof(serverAddr);

    struct timeval timeout;
    timeout.tv_sec = (udpTimeout-(udpTimeout%1000))/1000;
    timeout.tv_usec = (udpTimeout%1000)*1000;
    if (setsockopt(clientSocket_udp, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) 
    {
        fprintf(stderr, "ERROR: Error setting receive timeout\n");
        exit(EXIT_FAILURE);
    }


    
    main_pid_udp = getpid();
    pid_t pid;
    
    uint8_t retransmissions = 0;





    pid = fork();
    if(pid == 0) // listening
    {
        while(1)
        {
            bytesReceived = recvfrom(clientSocket_udp, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, &serverAddrLen);
            if (bytesReceived >= 0) 
            {
                buffer[bytesReceived] = '\0';
                decode_udp(&errorFlag, &sendErrorFlag, &repliedFlag, &byeFlag, &messageFlag, &confirmFlag, &successFlag, buffer, bytesReceived);
                if(!confirmFlag)
                {
                    confirmMessage[0] = 0x00;
                    confirmMessage[1] = buffer[1];
                    confirmMessage[2] = buffer[2];
                    ssize_t bytesSent = sendto(clientSocket_udp, confirmMessage, sizeof(confirmMessage), 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                    if (bytesSent == -1)
                    {
                        fprintf(stderr, "ERROR: Sending failed\n");
                    }
                }
                else
                {
                    retransmissions = 0;
                }
                pid = fork();
                if(pid == 0)
                {
                    // set flags

                    sem_wait(sem_udp);
                    uint16_t messageID_tmp;
                    messageID_tmp = (uint16_t)((buffer[2] << 8) | buffer[1]);

                    if(confirmFlag && messageID_tmp == *messageID - 1)
                    {
                        *confirmed = true;
                    }

                    if(repliedFlag)
                    {
                        *replied = true; 
                    }

                    if(successFlag && !(*authentized))
                    {
                        *authentized = true;
                    }

                    if(sendErrorFlag)
                    {
                        strcpy(message.content, "Message does not recognized");
                        create_message_udp(0xFE, toSend, &toSendLength, &message, messageID);
                        ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                        }
                        *messageID = *messageID + 1;
                    }
                    if(errorFlag)
                    {
                        create_message_udp(0xFF, toSend, &toSendLength, &message, messageID);
                        ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                            sem_close(sem_udp);
                            sem_unlink("connection_handler");
                            close(clientSocket_udp);
                            kill(0, SIGKILL);
                        }

                        sem_close(sem_udp);
                        sem_unlink("connection_handler");
                        close(clientSocket_udp);
                        kill(0, SIGKILL);
                    }
                    if(byeFlag)
                    {
                        sem_close(sem_udp);
                        sem_unlink("connection_handler");
                        close(clientSocket_udp);
                        kill(0, SIGKILL);
                    }

                    // set flags
                    sem_post(sem_udp);
                    exit(EXIT_SUCCESS);
                }
            }
            else if ((errno == EAGAIN || errno == EWOULDBLOCK) && !(*confirmed))
            {
                if(retransmissions >= udpMaxRetransmissions)
                {
                    create_message_udp(0xFF, toSend, &toSendLength, &message, messageID);
                    ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                    if (bytesSent == -1)
                    {
                        fprintf(stderr, "ERROR: Sending failed\n");
                        sem_close(sem_udp);
                        sem_unlink("connection_handler");
                        close(clientSocket_udp);
                        kill(0, SIGKILL);
                    }
                    sem_close(sem_udp);
                    sem_unlink("connection_handler");
                    close(clientSocket_udp);
                    kill(0, SIGKILL);
                }
                retransmissions++;
                ssize_t bytesSent = sendto(clientSocket_udp, toSend_shared, *toSendLength_shared, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                if (bytesSent == -1)
                {
                    fprintf(stderr, "ERROR: Sending failed\n");
                }
            }

            if(retransmissions >= udpMaxRetransmissions)
            {
                create_message_udp(0xFF, toSend, &toSendLength, &message, messageID);
                ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                if (bytesSent == -1)
                {
                    fprintf(stderr, "ERROR: Sending failed\n");
                    sem_close(sem_udp);
                    sem_unlink("connection_handler");
                    close(clientSocket_udp);
                    kill(0, SIGKILL);
                }
                sem_close(sem_udp);
                sem_unlink("connection_handler");
                close(clientSocket_udp);
                kill(0, SIGKILL);
            }
        }
    }
    else // sending
    {
        while(1)
        {
            sem_wait(sem_udp);
            if((*confirmed && *replied && *replyRequired) || (*confirmed && !(*replyRequired))) 
            {
                while(1)
                {
                    if(fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
                    {
                        create_message_udp(0xFF, toSend, &toSendLength, &message, messageID);
                        ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                        if (bytesSent == -1)
                        {
                            fprintf(stderr, "ERROR: Sending failed\n");
                            sem_close(sem_udp);
                            sem_unlink("connection_handler");
                            close(clientSocket_udp);
                            kill(0, SIGKILL);
                        }

                        sem_close(sem_udp);
                        sem_unlink("connection_handler");
                        close(clientSocket_udp);
                        kill(0, SIGKILL);
                    }
                    if(!set_message_udp(&notSendFlag, input, &message, &userNameRegex, &displayNameRegex, &secretTokenRegex, &messageContentRegex, &channelIdRegex))
                    {
                        message.type = 0x04;
                        strcpy(message.content, input);
                    }
                    if(!notSendFlag)
                    {
                        break;
                    }
                }

                if(message.type != 0x02 && !(*authentized))
                {
                    fprintf(stderr, "ERROR: Authentization needed\n");
                }
                else if(message.type == 0x02 && *authentized)
                {
                    fprintf(stderr, "ERROR: Already authentized\n");
                }
                else
                {
                    create_message_udp(message.type, toSend, &toSendLength, &message, messageID);
                    if(message.type == 0x02 || message.type == 0x03)
                    {
                        *replyRequired = true;
                    }
                    else
                    {
                        *replyRequired = false;
                    }

                    // Copy toSendLength to shared memory
                    *toSendLength_shared = toSendLength;

                    // Copy toSend to shared memory
                    memcpy(toSend_shared, toSend, toSendLength);

                    ssize_t bytesSent = sendto(clientSocket_udp, toSend, toSendLength, 0, (struct sockaddr *)&serverAddr, serverAddrLen);
                    if (bytesSent == -1)
                    {
                        fprintf(stderr, "ERROR: Sending failed\n");
                    }

                    *messageID = *messageID + 1;
                    *confirmed = false;
                    *replied = false;
                }
            }
            sem_post(sem_udp);
        }
    }
}



