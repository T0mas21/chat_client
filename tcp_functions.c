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
#include <regex.h>

#include "tcp_functions.h"

void print_help_tcp()
{
    printf("Supported local commands:\n");
    printf("/auth {Username} {Secret} {DisplayName}: Authenticate with the server\n");
    printf("/join {ChannelID}: Join a channel\n");
    printf("/rename {DisplayName}: Change the display name\n");
    printf("/help: Print this help message\n");
}



void create_message_tcp(char type[], char toSend[], size_t *toSendLength, Message *message)
{
    
    size_t offset = 0;
    
    if(strcmp(type, "AUTH") == 0)
    {
        offset = 0;
        strcpy(message->type, "AUTH");

        *toSendLength = strlen(message->type) + 4 + strlen(message->userName) 
                    + 1 + strlen(message->displayName) + 7 + strlen(message->secretToken) + 2;

        memcpy(toSend + offset, message->type, strlen(message->type));
        offset += strlen(message->type);

        toSend[offset++] = ' '; // Space terminator for type

        memcpy(toSend + offset, message->userName, strlen(message->userName));
        offset += strlen(message->userName);

        memcpy(toSend + offset, " AS ", strlen(" AS "));
        offset += strlen(" AS ");

        memcpy(toSend + offset, message->displayName, strlen(message->displayName));
        offset += strlen(message->displayName);

        memcpy(toSend + offset, " USING ", strlen(" USING "));
        offset += strlen(" USING ");

        memcpy(toSend + offset, message->secretToken, strlen(message->secretToken));
        offset += strlen(message->secretToken);
        
        toSend[offset++] = '\r'; // Carriage return terminator for secret token
        toSend[offset++] = '\n'; // Newline terminator for secret token

        toSend[offset] = '\0';
    }
    else if(strcmp(type, "JOIN") == 0)
    {
        offset = 0;
        strcpy(message->type, "JOIN");
        
        *toSendLength = strlen(message->type) + 1 + strlen(message->channelID) + 4 +
            strlen(message->displayName) + 2;

        memcpy(toSend + offset, message->type, strlen(message->type));
        offset += strlen(message->type);

        toSend[offset++] = ' ';

        memcpy(toSend + offset, message->channelID, strlen(message->channelID));
        offset += strlen(message->channelID);
           
        memcpy(toSend + offset, " AS ", strlen(" AS "));
        offset += strlen(" AS ");

        memcpy(toSend + offset, message->displayName, strlen(message->displayName));
        offset += strlen(message->displayName);

        toSend[offset++] = '\r'; 
        toSend[offset++] = '\n';     

        toSend[offset] = '\0';     
    }
    else if(strcmp(type, "MSG") == 0)
    {
        offset = 0;
        strcpy(message->type, "MSG");
        *toSendLength = strlen(message->type) + 6 +
          strlen(message->displayName) + 4 + strlen(message->content) + 2;
            
        memcpy(toSend + offset, message->type, strlen(message->type));
        offset += strlen(message->type);

        memcpy(toSend + offset, " FROM ", strlen(" FROM "));
        offset += strlen(" FROM ");

        memcpy(toSend + offset, message->displayName, strlen(message->displayName));
        offset += strlen(message->displayName);

        memcpy(toSend + offset, " IS ", strlen(" IS "));
        offset += strlen(" IS ");

        memcpy(toSend + offset, message->content, strlen(message->content));
        offset += strlen(message->content);

        toSend[offset++] = '\r'; 
        toSend[offset++] = '\n';  

        toSend[offset] = '\0';
    }
    else if(strcmp(type, "ERR") == 0)
    {
        offset = 0;
        strcpy(message->type, "ERR");
            
        *toSendLength = strlen(message->type) + 6 +
            strlen(message->displayName) + 4 + strlen(message->content) + 2;

        memcpy(toSend + offset, message->type, strlen(message->type));
        offset += strlen(message->type);

        memcpy(toSend + offset, " FROM ", strlen(" FROM "));
        offset += strlen(" FROM ");

        memcpy(toSend + offset, message->displayName, strlen(message->displayName));
        offset += strlen(message->displayName);

        memcpy(toSend + offset, " IS ", strlen(" IS "));
        offset += strlen(" IS ");

        memcpy(toSend + offset, message->content, strlen(message->content));
        offset += strlen(message->content);

        toSend[offset++] = '\r'; 
        toSend[offset++] = '\n'; 

        toSend[offset] = '\0';
    }
    else if(strcmp(type, "BYE") == 0)
    {
        offset = 0;
        strcpy(message->type, "BYE");
        
        *toSendLength = strlen(message->type) + 2;

        memcpy(toSend + offset, message->type, strlen(message->type));
        offset += strlen(message->type);

        toSend[offset++] = '\r'; 
        toSend[offset++] = '\n'; 

        toSend[offset] = '\0';
    }
}

bool set_message_tcp(bool *notSendFlag, char input[], Message *message, 
    regex_t *userNameRegex, regex_t *displayNameRegex, regex_t *secretTokenRegex, regex_t *messageContentRegex, regex_t *channelIdRegex)
{   
   
    int ret;
    *notSendFlag = false;
    // Remove newline character
    input[strcspn(input, "\n")] = '\0';

    int numArgsParsed;
    // Check if it's a local command or a message
    if (input[0] == '/') 
    {
        // Parse local command
        char command[10];
        sscanf(input, "/%s", command);

        if (strcmp(command, "auth") == 0) 
        {
            //message->type = 0x02;
            strcpy(message->type, "AUTH");
            // Parse parameters for /auth command
            numArgsParsed = sscanf(input, "/%*s %s %s %s", message->userName, message->displayName, message->secretToken);
            if(numArgsParsed != 3)
            {
                // message content regex control
                ret = regexec(messageContentRegex, input, 0, NULL, 0);
                if (ret) 
                {
                    fprintf(stderr, "Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control

            ret = regexec(userNameRegex, message->userName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid userName\n");
                *notSendFlag = true;
                return false;
            }

            ret = regexec(displayNameRegex, message->displayName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid displayName\n");
                *notSendFlag = true;
                return false;
            }

            ret = regexec(secretTokenRegex, message->secretToken, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid secretToken\n");
                *notSendFlag = true;
                return false;
            }

            // regex control

        } 
        else if (strcmp(command, "join") == 0) 
        {
            //message->type = 0x03;
            strcpy(message->type, "JOIN");
            // Parse parameters for /join command
            numArgsParsed = sscanf(input, "/%*s %s", message->channelID);
            if(numArgsParsed != 1)
            {
                // message content regex control
                ret = regexec(messageContentRegex, input, 0, NULL, 0);
                if (ret) 
                {
                    fprintf(stderr, "Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control
            ret = regexec(channelIdRegex, message->channelID, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid channel ID\n");
                *notSendFlag = true;
                return false;
            }
            // regex control

        } 
        else if (strcmp(command, "rename") == 0) 
        {
            // Parse parameters for /rename command
            numArgsParsed = sscanf(input, "/%*s %s", message->displayName);
            if(numArgsParsed != 1)
            {
                // message content regex control
                ret = regexec(messageContentRegex, input, 0, NULL, 0);
                if (ret) 
                {
                    fprintf(stderr, "Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control
            ret = regexec(displayNameRegex, message->displayName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid displayname\n");
                *notSendFlag = true;
                return false;
            }
            // regex control

            *notSendFlag = true;
        } 
        else if (strcmp(command, "help") == 0) 
        {
            // Print out supported local commands with descriptions
            print_help_tcp();
            *notSendFlag = true;
        } 
        else 
        {
            // message content regex control
            ret = regexec(messageContentRegex, input, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "Invalid message content\n");
                *notSendFlag = true;
            }
            // message content regex control
            return false;
        }
    }
    else
    {
        // message content regex control
        ret = regexec(messageContentRegex, input, 0, NULL, 0);
        if (ret) 
        {
            fprintf(stderr, "Invalid message content\n");
            *notSendFlag = true;
        }
        // message content regex control
        return false;
    } 
    return true;
}

void decode_tcp(bool *errorFlag, bool *sendErrorFlag, bool *repliedFlag, bool *byeFlag, bool *messageFlag, bool *succesFlag, char buffer[], int bytesReceived)
{
    int i = 0;

    char type[11];
    while(buffer[i] != ' ' && buffer[i] != '\0' && i < 10)
    {
        type[i] = buffer[i];
        i++;
    }
    type[i] = '\0';

    *repliedFlag = false;
    *succesFlag = false; 
    
    if(strcmp(type, "REPLY") == 0)
    {
        //*replied = true;
        *repliedFlag = true;
        if(buffer[6] == 'O' && buffer[7] == 'K')
        {
            printf("Success: ");
            for(i = 12; i < bytesReceived - 1; i++)
            {
                printf("%c", buffer[i]);
            }
            printf("\n");
            *succesFlag = true; 
        }
        else
        {
            printf("Failure: ");
            for(int i = 13; i < bytesReceived - 1; i++)
            {
                printf("%c", buffer[i]);
            }
            printf("\n");
            *succesFlag = false; 
        }
    }

    else if(strcmp(type, "MSG") == 0)
    { 
        //MSG FROM DisplayName IS MessageContent\r\n
        *messageFlag = true;
        i = 9;
        while(buffer[i] != ' ' && buffer[i] != '\0')
        {
            printf("%c", buffer[i]);
            i++;
        }
        printf(": ");
        for(int j = i + 4; j < bytesReceived - 1; j++)
        {
            printf("%c", buffer[j]);
        }
        printf("\n");
    }

    else if(strcmp(type, "ERR") == 0)
    {
        fprintf(stderr, "ERROR FROM ");
        i = 9;
        while(buffer[i] != ' ' && buffer[i] != '\0')
        {
            fprintf(stderr, "%c", buffer[i]);
            i++;
        }
        fprintf(stderr, ": ");
        for(int j = i + 4; j < bytesReceived - 1; j++)
        {
            fprintf(stderr, "%c", buffer[j]); // Use 'j' instead of 'i'
        }
        fprintf(stderr, "\n");
        *errorFlag = true;
    }

    else if(strcmp(type, "BYE") == 0)
    {
        *byeFlag = true;    
    }
    else
    {
        *errorFlag = true;
        *sendErrorFlag = true;
    }
}


