#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define PORT 7102
#define MAX_WORKERS 8
#define MAX_QUEUE_ENTRIES 1024
#define DEBUG
#define MASTER_FILE_PATH "cy_tdl_mf"
#define MAX_BUFFER_SIZE 2048
#define TTL 40

void getResponse(Message* query, Message* response);

pthread_mutex_t mutexQueriesQueue;
pthread_cond_t condQueriesQueue;
pthread_t workers[MAX_WORKERS];

typedef struct ComputeQuery {
    void (*computeQueryFunction) (int, Message, struct sockaddr_in);
    int socketDescriptor;
    Message recievedQuery;
    struct sockaddr_in querySender;
} ComputeQuery;

ComputeQuery querriesQueue[MAX_QUEUE_ENTRIES];
int querriesCount;

void computeQuery(int socketDescriptorR_NS, Message recievedQuery, struct sockaddr_in querySender) {
    Message response;
    (void) getResponse(&recievedQuery, &response);
    if (sendto(socketDescriptorR_NS, &response, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &querySender, sizeof(struct sockaddr)) < 0) {
        perror("Foreign Server> Could not send response to requester\n");
        exit(0);
    }
}

void executeComputeQuery(ComputeQuery* query) {
    query -> computeQueryFunction(query -> socketDescriptor, query -> recievedQuery, query -> querySender);
}

void submitQuery(ComputeQuery query) {
    pthread_mutex_lock(&mutexQueriesQueue);
    querriesQueue[querriesCount++] = query;
    pthread_mutex_unlock(&mutexQueriesQueue);
    pthread_cond_signal(&condQueriesQueue);
}

void* startWorker() {
    for (;;) {
        ComputeQuery query;
        pthread_mutex_lock(&mutexQueriesQueue);
        while (querriesCount == 0) {
            pthread_cond_wait(&condQueriesQueue, &mutexQueriesQueue);
        }
        query = querriesQueue[0];
        for (int i = 0; i < querriesCount - 1; ++i) {
            querriesQueue[i] = querriesQueue[i + 1];
        }
        --querriesCount;
        pthread_mutex_unlock(&mutexQueriesQueue);
        executeComputeQuery(&query);
    }
    return NULL;
}

int main() {
    struct sockaddr_in rootServer;
    struct sockaddr_in requester;
    Message recievedQuery;
    int socketDescriptor;

    pthread_mutex_init(&mutexQueriesQueue, NULL);
    pthread_cond_init(&condQueriesQueue, NULL);

    for (int i = 0; i < MAX_WORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, &startWorker, NULL) < 0) {
            perror("Foreign Server> Error: Could not create workers\n");
            exit(1);
        }
    }

    if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Foreign Server> Error: Could not create socket\n");
        exit(1);
    }

    bzero(&rootServer, sizeof(rootServer));
    bzero(&requester, sizeof(requester));

    rootServer.sin_family = AF_INET;
    rootServer.sin_addr.s_addr = htonl(INADDR_ANY);
    rootServer.sin_port = htons(PORT);

    if (bind(socketDescriptor, (struct sockaddr*) &rootServer, sizeof(rootServer)) < 0) {
        perror("Foreign Server> Error: Could not bind socket");
    }

    for (;;) {
        unsigned int requesterSize = sizeof(requester);
        if ((recvfrom(socketDescriptor, &recievedQuery, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &requester, &requesterSize)) < 0) {
            perror("Foreign Server> Error: Could not recieve query\n");
            exit(1);
        }
        ComputeQuery query;
        query.computeQueryFunction = &computeQuery;
        memcpy(&(query.querySender), &requester, sizeof(struct sockaddr_in));
        query.socketDescriptor = socketDescriptor;
        memcpy(&query.recievedQuery, &recievedQuery, MAX_MESSAGE_SIZE);
        submitQuery(query);
    }

    return 0;
}


void getResponse(Message* query, Message* response) {
    response -> header.id = query -> header.id;
    response -> header.qr = false;
    response -> header.aa = true;
    response -> header.rd = false;
    response -> header.ra = false;
    response -> header.anCount = 0;
    int masterFile = open(MASTER_FILE_PATH, O_RDONLY);
    if (masterFile < 0) {
        perror("Foreign server> Error: Could not open master file");
        exit(1);
    }
    char buffer[MAX_BUFFER_SIZE];
    int bufferLen = 0;
    if (read(masterFile, buffer, MAX_BUFFER_SIZE) < 0) {
        perror("Foreign Server> Error: Could not read master file");
        exit(1);
    }
    char* bufferLines = strtok(buffer, "\n");
    while (bufferLines) {
        switch (query->questionQType) {
            case A:
                if (bufferLines[0] == 'A' && bufferLines[1] == ' ') {
                    char domainName[MAX_NAME_SIZE];
                    int index = 1;
                    for (; bufferLines[index] == ' '; index++);
                    int domainNameIndex;
                    for(domainNameIndex = 0; bufferLines[index] != ' '; ++domainNameIndex) {
                        domainName[domainNameIndex] = bufferLines[index];
                        index++;
                    }
                    domainName[domainNameIndex] = '\0';
                    char domainNameCopy[MAX_NAME_SIZE];
                    for (int i = 0; query -> questionDomain[i]; ++i) {
                        domainNameCopy[i] = toupper(query -> questionDomain[i]);
                    }
                    domainNameCopy[strlen(query -> questionDomain) + 1] = '\0';
                    if (strcmp(domainName, domainNameCopy) == 0) {
                        for (; bufferLines[index] != '\0' && bufferLines[index] == ' '; index++);
                        if (bufferLines[index] != '\0') {
                            if (response -> header.anCount > MAX_ANSWER_SIZE) {
                                response -> header.tc = true;
                            } else {
                                int lenData = strlen(bufferLines + index);
                                char data[MAX_RRDATA];
                                data[lenData] = '\0';
                                for (int i = 0; bufferLines[index]; ++index, ++i) {
                                    data[i] = bufferLines[index];
                                }
                                unsigned short indexAnswerList = response -> header.anCount;
                                strcpy(response -> questionDomain, query -> questionDomain);
                                response -> answersList[indexAnswerList].rrClass = IN;
                                response -> answersList[indexAnswerList].rrType = A;
                                response -> answersList[indexAnswerList].rdLength = lenData;
                                response -> answersList[indexAnswerList].timeToLive = TTL + time(NULL);
                                strcpy(response -> answersList[indexAnswerList].rData, data);
                                response -> header.anCount++;
                            }
                        }
                    }
                } 
                break;
            case NS:
                if (bufferLines[0] == 'N' && bufferLines[1] == 'S' && bufferLines[2] == ' ') {
                    int index = 2;
                    for (; bufferLines[index] == ' '; ++index);
                    char domainName[MAX_MESSAGE_SIZE];
                    int domainNameLen = 0;
                    for (; bufferLines[index] != '\0'; ++index) {
                        domainName[domainNameLen++] = bufferLines[index];
                    }
                    domainName[domainNameLen] = '\0';
                    strcpy(response -> questionDomain, query -> questionDomain);
                    if (response -> header.anCount > MAX_ANSWER_SIZE) {
                        response -> header.tc = true;
                    } else {
                        strcpy(response -> answersList[response -> header.anCount].rData, domainName);
                        response -> answersList[response -> header.anCount].rdLength = strlen(domainName);
                        response -> answersList[response -> header.anCount].rrClass = IN;
                        response -> answersList[response -> header.anCount].rrType = NS;
                        response -> answersList[response -> header.anCount].timeToLive = TTL + time(NULL);
                        response -> header.anCount++;
                    }
                }
                break;
            case CNAME:
                response -> header.rcode = 4;
                break;
            default:
                break;
        }
    bufferLines = strtok(NULL, "\n");
    }
    if (response -> header.rcode != 4) {
        if (response -> header.anCount == 0) {
            response -> header.rcode = 3;
        } else {
            response -> header.rcode = 0;
        }
    }
}