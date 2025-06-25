#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h> 
#include <errno.h> 


#include "tcp.h"
#include "udp.h"

int gobalni;

void help(char *argv[])
{
    printf("Usage: %s -t <tcp/udp> -s <server_address> [-p <server_port>] [-d <timeout>] [-r <retransmissions>] [-h]\n", argv[0]);
}


int main(int argc, char *argv[])
{
    char *transportProtocol = NULL;
    char *serverAddress = NULL;
    __uint16_t serverPort = 4567; 
    __uint16_t udpTimeout = 250;
    __uint8_t udpMaxRetransmissions = 3;

    char *endptr;
    long value;

    int opt;
     while ((opt = getopt(argc, argv, "t:s:p:d:r:h")) != -1) 
     {
        switch (opt) {
            case 't':
                transportProtocol = optarg;
                if (strcmp(transportProtocol, "udp") != 0 && strcmp(transportProtocol, "tcp") != 0) 
                {
                    fprintf(stderr, "ERROR: Invalid transport protocol '%s'\n", transportProtocol);
                    help(argv);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                serverAddress = optarg;
                break;
            case 'p': 
                value = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || value < 0) 
                {
                    fprintf(stderr, "ERROR: Invalid port number '%s'\n", optarg);
                    exit(EXIT_FAILURE);
                }
                serverPort = (__uint16_t)value;
                break;
            case 'd':
                value = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || value < 0) 
                {
                    fprintf(stderr, "ERROR: Invalid udp confirmation timeout number '%s'\n", optarg);
                    exit(EXIT_FAILURE);
                }
                udpTimeout  = (__uint16_t)value;
                break;
            case 'r':
                value = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || value < 0) 
                {
                    fprintf(stderr, "ERROR: Invalid udp maximum retransmissions number '%s'\n", optarg);
                    exit(EXIT_FAILURE);
                }
                udpMaxRetransmissions  = (__uint8_t)value;
                break;
            case 'h':
                help(argv);
                return 0;
            case '?':
                if (optopt == 't' || optopt == 's' || optopt == 'p' || optopt == 'd' || optopt == 'r') 
                {
                    fprintf(stderr, "ERROR: Option -%c requires an argument.\n", optopt);
                } 
                else if (isprint(optopt)) 
                {
                    fprintf(stderr, "ERROR: Unknown option `-%c'.\n", optopt);
                } 
                else 
                {
                    fprintf(stderr, "ERROR: Unknown option character `\\x%x'.\n", optopt);
                }
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }

    if(transportProtocol == NULL || serverAddress == NULL)
    {
        fprintf(stderr, "ERROR: Transport protocol and server address are mandatory.\n");
        exit(EXIT_FAILURE);
    }

    if(strcmp(transportProtocol, "udp") == 0)
    {
        myUDPconnection(serverAddress, serverPort, udpTimeout, udpMaxRetransmissions);
    }
    else
    {
        myTCPconnection(serverAddress, serverPort);
    }
    
    return 0;
}




