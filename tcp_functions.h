#ifndef TCP_FUNCTIONS_H
#define TCP_FUNCTIONS_H

#include <stdbool.h>
#include <stddef.h>

#define BUFFER_SIZE 1024
#define MAX_MESSAGE_SIZE 1458

typedef struct {
    char type[10];
    //uint16_t messageID;
    char userName[20];
    char displayName[20];
    char channelID[20];
    char secretToken[128];
    char content[1400];
} Message;

void print_help_tcp();
void create_message_tcp(char type[], char toSend[], size_t *toSendLength, Message *message);
bool set_message_tcp(bool *notSendFlag, char input[], Message *message,
 regex_t *userNameRegex, regex_t *displayNameRegex, regex_t *secretTokenRegex, regex_t *messageContentRegex, regex_t *channelIdRegex);
void decode_tcp(bool *errorFlag, bool *sendErrorFlag, bool *repliedFlag, bool *byeFlag, bool *messageFlag, bool *succesFlag, char buffer[], int bytesReceived);



#endif /* TCP_FUNCTIONS_H */