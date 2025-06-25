#ifndef UDP_FUNCTIONS_H
#define UDP_FUNCTIONS_H


#define BUFFER_SIZE 1024
#define MAX_MESSAGE_SIZE 1458

typedef struct {
    uint8_t type;
    //uint16_t messageID;
    char userName[20];
    char displayName[20];
    char channelID[20];
    char secretToken[128];
    char content[1400];
} Message;


void print_help_udp();
void create_message_udp(uint8_t type, uint8_t toSend[], size_t *toSendLength, Message *message, uint16_t *messageID);
bool set_message_udp(bool *notSendFlag, char input[], Message *message,
 regex_t *userNameRegex, regex_t *displayNameRegex, regex_t *secretTokenRegex, regex_t *messageContentRegex, regex_t *channelIdRegex);
void decode_udp(bool *errorFlag, bool *sendErrorFlag, bool *repliedFlag, bool *byeFlag, bool *messageFlag, bool *confirmFlag, bool *successFlag, uint8_t buffer[], int bytesReceived);



#endif /* UDP_FUNCTIONS_H */