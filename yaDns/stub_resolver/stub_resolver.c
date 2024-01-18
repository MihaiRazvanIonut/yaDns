#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define MAX_CONFIG_BUFFER 32
#define MAX_IP_BUFFER_SIZE 16


// Recognising recursive resolver 
void readResolverConfig(char* ipV4, int* port);

int main() {
    int recursiveResolverPort;
    char recursiveResolverAdress[MAX_IP_BUFFER_SIZE];
    (void) readResolverConfig(recursiveResolverAdress, &recursiveResolverPort);

    int socketDescriptor;
    struct sockaddr_in recursiveResolver;
    int recursiveResolverLength;
    char question[MAX_QUESTION_SIZE];
    int questionLength = 0;
    struct QuestionInfo sentQuestionInfo;
    bzero(&sentQuestionInfo, sizeof(sentQuestionInfo));
    char answer[MAX_MESSAGE_SIZE];

    if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("StubResolver> Error: Could not create socket\n");
        return errno;
    }

    recursiveResolver.sin_family = AF_INET;
    recursiveResolver.sin_addr.s_addr = inet_addr(recursiveResolverAdress);
    recursiveResolver.sin_port = htons(recursiveResolverPort);

    // readQuestion(question, &questionLength);
    read(0, question, MAX_QUESTION_SIZE);

    recursiveResolverLength = sizeof(recursiveResolver);
    question[strlen(question) - 1] = '\0';
    if (sendto(socketDescriptor, question, MAX_QUESTION_SIZE, 0, (struct sockaddr*) &recursiveResolver, recursiveResolverLength) < 0) {
        perror("StubResolver> Error: Could not send question to resolver\n");
        return errno;
    }

    if ((recvfrom(socketDescriptor, answer, MAX_MESSAGE_SIZE, 0, (struct sockaddr*)&recursiveResolver, &recursiveResolverLength)) < 0) {
        perror("StubResolver> Error: Could not get answer from resolver");
        return errno;
    }
    printf("%s", answer);
    return 0;
}

void readResolverConfig(char* ipV4, int* port) {
    int configFd;
    if ((configFd = open("stub_resolver_config.csv", O_RDONLY)) < 0) {
        perror("StubResolver> Error: Could not open in read-only config file\n");
        exit(1);
    }
    char configBuffer[MAX_CONFIG_BUFFER];
    if ((read(configFd, configBuffer, MAX_CONFIG_BUFFER)) < 0) {
        perror("StubResolver> Error: Could not read from config file\n");
        exit(1);
    }
    char* token = strtok(configBuffer, ",");
    strcpy(ipV4, token);
    token = strtok(NULL, ",");
    *port = atoi(token);
}