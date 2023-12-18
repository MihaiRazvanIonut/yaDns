#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define PORT 7100
#define MAX_WORKERS 8
#define MAX_QUEUE_ENTRIES 1024
#define DEBUG

pthread_mutex_t mutexQueriesQueue;
pthread_cond_t condQueriesQueue;
pthread_t workers[MAX_WORKERS];

typedef struct ComputeQuery {
    void (*computeQueryFunction) (int, char[], struct sockaddr_in);
    int socketDescriptor;
    char recievedQuery[MAX_MESSAGE_SIZE];
    struct sockaddr_in querySender;
} ComputeQuery;

ComputeQuery querriesQueue[MAX_QUEUE_ENTRIES];
int querriesCount;

void computeQuery(int socketDescriptorR_NS, char recievedQuery[], struct sockaddr_in querySender) {

    // Dummy code to test functionality
    char response[MAX_MESSAGE_SIZE];
    bzero(&response, MAX_MESSAGE_SIZE);
    strcpy(response, recievedQuery);
    strcat(response, ":Seen by Root Server:");
    printf("Recieved query %s\n", recievedQuery);
    if (sendto(socketDescriptorR_NS, response, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &querySender, sizeof(struct sockaddr)) < 0) {
        perror("Root> Could not send response to requester\n");
        exit(0);
    }
    // end of dummy code
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
    char recievedQuery[MAX_MESSAGE_SIZE];
    int socketDescriptor;

    pthread_mutex_init(&mutexQueriesQueue, NULL);
    pthread_cond_init(&condQueriesQueue, NULL);

    for (int i = 0; i < MAX_WORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, &startWorker, NULL) < 0) {
            perror("Root> Error: Could not create workers\n");
            exit(1);
        }
    }

    if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Root> Error: Could not create socket\n");
        exit(1);
    }

    bzero(&rootServer, sizeof(rootServer));
    bzero(&requester, sizeof(requester));

    rootServer.sin_family = AF_INET;
    rootServer.sin_addr.s_addr = htonl(INADDR_ANY);
    rootServer.sin_port = htons(PORT);

    if (bind(socketDescriptor, (struct sockaddr*) &rootServer, sizeof(rootServer)) < 0) {
        perror("Root> Error: Could not bind socket");
    }

    for (;;) {
        unsigned int requesterSize = sizeof(requester);
        bzero(&recievedQuery, MAX_MESSAGE_SIZE);
        if ((recvfrom(socketDescriptor, &recievedQuery, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &requester, &requesterSize)) < 0) {
            perror("Root> Error: Could not recieve query\n");
            exit(1);
        }
        ComputeQuery query;
        query.computeQueryFunction = &computeQuery;
        memcpy(&(query.querySender), &requester, sizeof(struct sockaddr_in));
        query.socketDescriptor = socketDescriptor;
        strcpy(query.recievedQuery, recievedQuery);
        submitQuery(query);
    }

    return 0;
}