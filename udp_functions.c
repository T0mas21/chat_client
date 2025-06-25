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

#include "udp_functions.h"


void print_help_udp()
{
    printf("Supported local commands:\n");
    printf("/auth {Username} {Secret} {DisplayName}: Authenticate with the server\n");
    printf("/join {ChannelID}: Join a channel\n");
    printf("/rename {DisplayName}: Change the display name\n");
    printf("/help: Print this help message\n");
}


void create_message_udp(uint8_t type, uint8_t toSend[], size_t *toSendLength, Message *message, uint16_t *messageID)
{
    size_t offset = 0;
    switch(type)
    {
        case 0x02: // AUTH
            offset = 0;
            message->type = type;
            *toSendLength = sizeof(message->type) + sizeof(*messageID) +
                      strlen(message->userName) + 1 + strlen(message->displayName) + 1 + strlen(message->secretToken) + 1;
    

            memcpy(toSend + offset, &message->type, sizeof(message->type));
            offset += sizeof(message->type);

            memcpy(toSend + offset, messageID, sizeof(*messageID));
            offset += sizeof(*messageID);
            
            memcpy(toSend + offset, message->userName, strlen(message->userName));
            offset += strlen(message->userName);
            
            toSend[offset++] = 0; // Null terminator for username

            memcpy(toSend + offset, message->displayName, strlen(message->displayName));
            offset += strlen(message->displayName);

            toSend[offset++] = 0; // Null terminator for displayname

            memcpy(toSend + offset, message->secretToken, strlen(message->secretToken));
            offset += strlen(message->secretToken);
            
            toSend[offset++] = 0; // Null terminator for secret token
        break;

        case 0x03: // JOIN
            offset = 0;
            message->type = type;
            *toSendLength = sizeof(message->type) + sizeof(*messageID) +
                      strlen(message->channelID) + 1 + strlen(message->displayName) + 1;

            memcpy(toSend + offset, &message->type, sizeof(message->type));
            offset += sizeof(message->type);

            //*messageID = swap_bytes(*messageID);
            memcpy(toSend + offset, messageID, sizeof(*messageID));
            offset += sizeof(*messageID);

            memcpy(toSend + offset, message->channelID, strlen(message->channelID));
            offset += strlen(message->channelID);

            toSend[offset++] = 0; // Null terminator for username

            memcpy(toSend + offset, message->displayName, strlen(message->displayName));
            offset += strlen(message->displayName);

            toSend[offset++] = 0; // Null terminator for displayname
        break;

        case 0x04: // MSG
            offset = 0;
            message->type = type;
            *toSendLength = sizeof(message->type) + sizeof(*messageID) +
                      strlen(message->displayName) + 1 + strlen(message->content) + 1;
            
            memcpy(toSend + offset, &message->type, sizeof(message->type));
            offset += sizeof(message->type);

            memcpy(toSend + offset, messageID, sizeof(*messageID));
            offset += sizeof(*messageID);

            memcpy(toSend + offset, message->displayName, strlen(message->displayName));
            offset += strlen(message->displayName);

            toSend[offset++] = 0; // Null terminator for displayname

            memcpy(toSend + offset, message->content, strlen(message->content));
            offset += strlen(message->content);

            toSend[offset++] = 0; // Null terminator for content

            //*messageID = swap_bytes(*messageID);
        break;

        case 0xFE: // ERR
            offset = 0;
            message->type = type;
            *toSendLength = sizeof(message->type) + sizeof(*messageID) +
                      strlen(message->displayName) + 1 + strlen(message->content) + 1;

            memcpy(toSend + offset, &message->type, sizeof(message->type));
            offset += sizeof(message->type);

            //*messageID = swap_bytes(*messageID);
            memcpy(toSend + offset, messageID, sizeof(*messageID));
            offset += sizeof(*messageID);

            memcpy(toSend + offset, message->displayName, strlen(message->displayName));
            offset += strlen(message->displayName);

            toSend[offset++] = 0; // Null terminator for displayname

            memcpy(toSend + offset, message->content, strlen(message->content));
            offset += strlen(message->content);

            toSend[offset++] = 0; // Null terminator for content

        break;

        case 0xFF: // BYE
            offset = 0;
            message->type = type;
            *toSendLength = sizeof(message->type) + sizeof(*messageID);

            memcpy(toSend + offset, &message->type, sizeof(message->type));
            offset += sizeof(message->type);

            memcpy(toSend + offset, messageID, sizeof(*messageID));
            offset += sizeof(*messageID);

            //*messageID = swap_bytes(*messageID);
        break;
    }
}


bool set_message_udp(bool *notSendFlag, char input[], Message *message,
 regex_t *userNameRegex, regex_t *displayNameRegex, regex_t *secretTokenRegex, regex_t *messageContentRegex, regex_t *channelIdRegex)
{
    /*if(fgets(input, inputSize, stdin) == NULL)
    {
        *eofFlag = true;
        return false;
    }*/


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
            message->type = 0x02;
            // Parse parameters for /auth command
            numArgsParsed = sscanf(input, "/%*s %s %s %s", message->userName, message->displayName, message->secretToken);
            
            if(numArgsParsed != 3)
            {
                // message content regex control
                ret = regexec(messageContentRegex, input, 0, NULL, 0);
                if (ret) 
                {
                    fprintf(stderr, "ERROR: Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control

            ret = regexec(userNameRegex, message->userName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid userName\n");
                *notSendFlag = true;
                return false;
            }

            ret = regexec(displayNameRegex, message->displayName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid displayName\n");
                *notSendFlag = true;
                return false;
            }

            ret = regexec(secretTokenRegex, message->secretToken, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid secretToken\n");
                *notSendFlag = true;
                return false;
            }

            // regex control

        } 
        else if (strcmp(command, "join") == 0) 
        {
            message->type = 0x03;
            // Parse parameters for /join command
            numArgsParsed = sscanf(input, "/%*s %s", message->channelID);

            if(numArgsParsed != 1)
            {
                // message content regex control
                ret = regexec(messageContentRegex, input, 0, NULL, 0);
                if (ret) 
                {
                    fprintf(stderr, "ERROR: Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control
            ret = regexec(channelIdRegex, message->channelID, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid channel ID\n");
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
                    fprintf(stderr, "ERROR: Invalid message content\n");
                    *notSendFlag = true;
                }
                // message content regex control
                return false;
            }

            // regex control
            ret = regexec(displayNameRegex, message->displayName, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid displayname\n");
                *notSendFlag = true;
                return false;
            }
            // regex control

            *notSendFlag = true;
        } 
        else if (strcmp(command, "help") == 0) 
        {
            // Print out supported local commands with descriptions
            print_help_udp();
            *notSendFlag = true;
        } 
        else 
        {
            // message content regex control
            ret = regexec(messageContentRegex, input, 0, NULL, 0);
            if (ret) 
            {
                fprintf(stderr, "ERROR: Invalid message content\n");
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
            fprintf(stderr, "ERROR: Invalid message content\n");
            *notSendFlag = true;
        }
        // message content regex control
        return false;
    } 
    return true;
}
    


void decode_udp(bool *errorFlag, bool *sendErrorFlag, bool *repliedFlag, bool *byeFlag, bool *messageFlag, bool *confirmFlag, bool *successFlag, uint8_t buffer[], int bytesReceived)
{
    int i;
    *confirmFlag = false;
    *repliedFlag = false;
    *successFlag = false;
    switch (buffer[0])
    {
    case 0x00:
        *confirmFlag = true;
        break;

    case 0x01:
        *repliedFlag = true;
        if(buffer[3] == 0x01)
        {
            printf("Success: ");
            for(i = 6; i < bytesReceived - 1; i++)
            {
                printf("%c", buffer[i]);
            }
            printf("\n");
            *successFlag = true;
        }
        else
        {
            printf("Failure: ");
            for(int i = 6; i < bytesReceived - 1; i++)
            {
                printf("%c", buffer[i]);
            }
            printf("\n");
            *successFlag = false;
        }
        break;

    case 0x04:
        *messageFlag = true;
        i = 3;
        while(buffer[i] != 0x00)
        {
            printf("%c", buffer[i]);
            i++;
        }
        printf(": ");
        for(int j = i; j < bytesReceived - 1; j++)
        {
            printf("%c", buffer[j]);
        }
        printf("\n");
        break;

    case 0xFE:
       fprintf(stderr, "ERROR FROM ");
       i = 3;
        while(buffer[i] != 0x00)
        {
            fprintf(stderr, "%c", buffer[i]);
            i++;
        }
        fprintf(stderr, ": ");
        for(int j = i; j < bytesReceived - 1; j++)
        {
            fprintf(stderr, "%c", buffer[j]); // Use 'j' instead of 'i'
        }
        fprintf(stderr, "\n");
        *errorFlag = true;
        break;

    case 0xFF:
        *byeFlag = true;
        break;    

    default:
        *errorFlag = true;
        *sendErrorFlag = true;
        break;
    }
}