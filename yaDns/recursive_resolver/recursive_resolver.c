#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define PORT 7710
#define MAX_WORKERS 8
#define MAX_QUEUE_ENTRIES 1024
#define DEBUG

pthread_mutex_t mutexQuestionsQueue;
pthread_cond_t condQuestionsQueue;
pthread_t workers[MAX_WORKERS];

typedef struct ResolveQuestion {
    void (*resolverFunction) (int, char[], struct sockaddr_in);
    int socketDescriptor;
    char recievedQuestion[MAX_MESSAGE_SIZE];
    struct sockaddr_in questionSender;
} ResolveQuestion;

ResolveQuestion questionsQueue[MAX_QUEUE_ENTRIES];
int questionsCount;

void resolveQuestion(int socketDescriptorS_R, char recievedQuestion[], struct sockaddr_in questionSender) {
    /**
     *  Message queryMessage;
     *  Message responseMessage;
     *  
     *  bzero(&queryMessage, sizeof(Message));
     *  bzero(&responseMessage, sizeof(Message));
     */

    #ifdef DEBUG
    printf("Resolver> Recieved question...\n");
    #endif
    /**
     *  todo!()
     *  Format the question into a standard query
     *  Check cache to see if query was already resolved
     *  formatQuestionIntoQuery(QuestionInfo* question, Message* query);
     */

    // Dummy code to send a querryMessage to a foreign server
    int socketDescriptorR_NS;
    struct sockaddr_in foreignServer;
    if ((socketDescriptorR_NS = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Resolver> Error: Could not create socket to foreign server\n");
        exit(1);
    }
    foreignServer.sin_family = AF_INET;
    foreignServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    foreignServer.sin_port = htons(7100);
    unsigned int lengthForeignServer = sizeof(foreignServer);
    char formatedQuery[MAX_MESSAGE_SIZE];
    char response[MAX_MESSAGE_SIZE];
    strcpy(formatedQuery, recievedQuestion);
    strcat(formatedQuery, ":Seen by Resolver:");
    if (sendto(socketDescriptorR_NS, formatedQuery, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &foreignServer, lengthForeignServer) <= 0) {
        perror("Resolver> Error: Could not send message to foreign server\n");
        exit(1);
    }
    if ((recvfrom(socketDescriptorR_NS, response, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &foreignServer, &lengthForeignServer)) < 0) {
        perror("Resolver> Error: Could not recieve message from foreign server\n");
        exit(1);
    }
    close(socketDescriptorR_NS);
    // end of dummy code

    /**
     *  todo!()
     *  Based on the answer either return it to the question sender or open new socket to the appropiate server (recursive search)
     */
    #ifdef DEBUG
    printf("Resolver> Sending back answer...\n");
    #endif
    /**
     * todo!()
     * Format response from foreign server into a easy to read string
    */
    if (sendto(socketDescriptorS_R, response, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &questionSender, sizeof(struct sockaddr)) <= 0) {
        perror("Resolver> Error: Could not send answer back to sender\n");
        exit(1);
    }
    #ifdef DEBUG 
    else {
          printf("Resolver> Answer sent back with succes!\n");  
    }
    #endif
}

void executeResolveQuestion(ResolveQuestion* question) {
    question -> resolverFunction(question -> socketDescriptor, question -> recievedQuestion, question -> questionSender);
}

void submitQuestion(ResolveQuestion question) {
    pthread_mutex_lock(&mutexQuestionsQueue);
    questionsQueue[questionsCount++] = question;
    pthread_mutex_unlock(&mutexQuestionsQueue);
    pthread_cond_signal(&condQuestionsQueue);
}

void* startWorker() {
    for (;;) {
        ResolveQuestion question;
        pthread_mutex_lock(&mutexQuestionsQueue);
        while (questionsCount == 0) {
            pthread_cond_wait(&condQuestionsQueue, &mutexQuestionsQueue);
        }
        question = questionsQueue[0];
        for (int i = 0; i < questionsCount - 1; ++i) {
            questionsQueue[i] = questionsQueue[i + 1];
        }
        --questionsCount;
        pthread_mutex_unlock(&mutexQuestionsQueue);
        executeResolveQuestion(&question);
    }
    return NULL;
}

int main() {
    struct sockaddr_in resolver;
    struct sockaddr_in stubResolver;
    char recievedQuestion[MAX_MESSAGE_SIZE];
    int socketDescriptor;
    pthread_mutex_init(&mutexQuestionsQueue, NULL);
    pthread_cond_init(&condQuestionsQueue, NULL);

    for (int i = 0; i < MAX_WORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, &startWorker, NULL) < 0) {
            perror("Resolver> Error: Could not create workers\n");
            exit(1);
        }
    }

    if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Resolver> Error: Could not create socket\n");
        exit(1);
    }

    bzero(&resolver, sizeof(resolver));
    bzero(&stubResolver, sizeof(resolver));

    resolver.sin_family = AF_INET;
    resolver.sin_addr.s_addr = htonl(INADDR_ANY);
    resolver.sin_port = htons(PORT);

    if (bind(socketDescriptor, (struct sockaddr*) &resolver, sizeof(struct sockaddr)) == -1) {
        perror("Resolver> Error: Could not bind socket\n");
        exit(1);
    }

    for (;;) {
        unsigned int stubResolverSize = sizeof(stubResolver);
        bzero(&recievedQuestion, sizeof(QuestionInfo));

        if ((recvfrom(socketDescriptor, &recievedQuestion, sizeof(QuestionInfo), 0, (struct sockaddr*) &stubResolver, &stubResolverSize)) <= 0 ) {
            perror("Resolver> Error: Could not recieve question from stub resolver\n");
            exit(1);
        }
        ResolveQuestion questionToBeResolved;
        strcpy(questionToBeResolved.recievedQuestion, recievedQuestion);
        memcpy(&(questionToBeResolved.questionSender), &stubResolver, sizeof(struct sockaddr_in));
        questionToBeResolved.socketDescriptor = socketDescriptor;
        questionToBeResolved.resolverFunction = &resolveQuestion;
        submitQuestion(questionToBeResolved);
    }

    /**
     * Cleanup code (to be used :P)
     * 
     * for (int i = 0; i < MAX_WORKERS; ++i) {
     *     pthread_join(workers[i], NULL);
     * }
     * pthread_mutex_destroy(&mutexQuestionsQueue);
     * pthread_cond_destroy(&condQuestionsQueue);
     * close(socketDescriptor);
    */
    return 0;
}