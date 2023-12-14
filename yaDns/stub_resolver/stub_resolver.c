#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define MAX_CONFIG_BUFFER 32
#define MAX_IP_BUFFER_SIZE 16
#define MAX_QUESTION_SIZE 268   // domain name (255 octets) + " " +
                                // qType (2 byte integer)   + " " +
                                // qClass (2 byte integer)  + "\0" 

typedef struct QuestionInfo {
    int questionCategory;
    char domainName[MAX_NAME_SIZE];
    enum QType qType;
    enum QClass qClass;
} QuestionInfo;

// Recognising recursive resolver
int readResolverConfig(char* ipV4, int* port);

extern int errno;


int main() {
    int recursiveResolverPort;
    char recursiveResovlerAdress[MAX_IP_BUFFER_SIZE];
    (void) readResolverConfig(recursiveResovlerAdress, &recursiveResolverPort);

    int socketDescriptor;
    struct sockaddr_in recursiveResovler;
    int recursiveResolverLength;
    char question[MAX_QUESTION_SIZE];
    int questionLength = 0;
    QuestionInfo questionInfo;
    bzero(&questionInfo, sizeof(questionInfo));
    char answer[MAX_MESSAGE_SIZE];

    if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("StubResolver> Error: Could not create socket!\n");
        return errno;
    }

    recursiveResovler.sin_family = AF_INET;
    recursiveResovler.sin_addr.s_addr = inet_addr(recursiveResovlerAdress);
    recursiveResovler.sin_port = htons(recursiveResolverPort);

    // readQuestion(question, &questionLength);
    // parseQuestion(&questionInfo, question, questionLength);

    recursiveResolverLength = sizeof(recursiveResovler);

    if (sendto(socketDescriptor, &questionInfo, sizeof(questionInfo), 0, (struct sockaddr*) &recursiveResovler, recursiveResolverLength) < 0) {
        perror("StubResolver> Error: Could not send question to resolver!\n");
        return errno;
    }

    if ((recvfrom(socketDescriptor, answer, MAX_MESSAGE_SIZE, 0, (struct sockaddr*)&recursiveResovler, &recursiveResolverLength)) < 0) {
        perror("StubResolver> Error: Could not get answer from resolver");
        return errno;
    }
    return 0;
}

int readResolverConfig(char* ipV4, int* port) {
    int configFd;
    if ((configFd = open("stub_resolver_config.csv", O_RDONLY)) < 0) {
        perror("StubResolver> Error: Could not open in read-only config file!\n");
        return errno;
    }
    char configBuffer[MAX_CONFIG_BUFFER];
    if ((read(configFd, configBuffer, MAX_CONFIG_BUFFER)) < 0) {
        perror("StubResolver> Error: Could not read from config file!\n");
        return errno;
    }
    char* token = strtok(configBuffer, ",");
    strcpy(ipV4, token);
    token = strtok(NULL, ",");
    *port = atoi(token);
}