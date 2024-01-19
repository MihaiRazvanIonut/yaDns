#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define PORT 7710
#define MAX_WORKERS 4
#define MAX_QUEUE_ENTRIES 1024
#define SLIST_SIZE 3
#define MAX_CACHE_SIZE 10
#define DEBUG
pthread_mutex_t mutexCache;
pthread_t chacheThread;
ResourceRecordCached cache[MAX_CACHE_SIZE];
int cacheSize = 0;
int checkCache(Message* message) {
    pthread_mutex_lock(&mutexCache);
    for (int i = 0; i < cacheSize; ++i) {
        if (strcmp(cache[i].domainName, message->questionDomain) == 0 && cache[i].rr.rrType == message->questionQType) {
            pthread_mutex_unlock(&mutexCache);
            return 0;
        }
    }
    pthread_mutex_unlock(&mutexCache);
    return -1;
}
void buildResponseFromCache(Message* response, Message* query) {
    pthread_mutex_lock(&mutexCache);
    response -> header.anCount = 0;
    for (int i = 0; i < cacheSize; ++i) {
        if (strcmp(cache[i].domainName, query->questionDomain) == 0 && cache[i].rr.rrType == query->questionQType) {
            printf("I get here: %d, %d\n", query->questionQType, cache[i].rr.rrType);
            printf("I get here: %s, %s\n", query->questionDomain, cache[i].domainName);
            response -> header.aa = false;
            response -> header.id = query -> header.id;
            response -> header.qr = true;
            response -> header.rcode = 0;
            response -> header.ra = false;
            response -> header.rd = false;
            response -> header.tc = false;
            response -> questionQType = query -> questionQType;
            strcpy(response->questionDomain, query->questionDomain);
            response -> answersList[response->header.anCount].rrType = response -> questionQType;
            response -> answersList[response->header.anCount].rrClass = IN;
            response -> answersList[response->header.anCount].rdLength = cache[i].rr.rdLength;
            strcpy(response -> answersList[response->header.anCount].rData, cache[i].rr.rData);
            response -> header.anCount++;
        }
    }
    pthread_mutex_unlock(&mutexCache);
}
void cacheResponse(Message* message) {
    pthread_mutex_lock(&mutexCache);
    if (cacheSize == MAX_CACHE_SIZE) {
        cacheSize -= message->header.anCount;
    } 
    for (int i = 0; cacheSize < MAX_CACHE_SIZE && i < message -> header.anCount; ++i, ++cacheSize) {
        strcpy(cache[cacheSize].domainName, message->questionDomain);
        strcpy(cache[cacheSize].rr.rData, message->answersList[i].rData);
        cache[cacheSize].rr.rdLength = message->answersList[i].rdLength;
        cache[cacheSize].rr.rrClass = message->answersList[i].rrClass;
        cache[cacheSize].rr.rrType = message->answersList[i].rrType;
        cache[cacheSize].rr.timeToLive = message->answersList[i].timeToLive;
    }
    pthread_mutex_unlock(&mutexCache);
}
void* cacheMaintenance() {
    for (;;) {
        for (int i = 0; i < cacheSize; ++i) {
            time_t now = time(NULL);
            if (cache[i].rr.timeToLive < now) {
                pthread_mutex_lock(&mutexCache);
                printf("I got here: %ld, %ld\n", cache[i].rr.timeToLive, now);
                for (int j = i; j < cacheSize - 1; ++j) {
                    cache[j] = cache[j + 1];
                }
                cacheSize--;
                pthread_mutex_unlock(&mutexCache);
            }
        }
    }
}

typedef struct NSInfo {
    int port;
    char domainName[MAX_NAME_SIZE];
} NSInfo;

NSInfo sList [SLIST_SIZE];
void loadNameServerList();
int get_appropiate_port(char domainName[MAX_NAME_SIZE]);
QType parse_recieved_question(char question[MAX_QUESTION_SIZE], char domainName[MAX_NAME_SIZE]);
void formatQuestionIntoQuery(char domainName[MAX_NAME_SIZE], QType QType, Message* query);
void buildNotRecognisedResponse(Message* response, Message* query);
pthread_mutex_t mutexQuestionsQueue;
pthread_cond_t condQuestionsQueue;
pthread_t workers[MAX_WORKERS];

typedef struct ResolveQuestion {
    void (*resolverFunction) (int, char[], struct sockaddr_in);
    int socketDescriptor;
    char recievedQuestion[MAX_QUESTION_SIZE];
    struct sockaddr_in questionSender;
} ResolveQuestion;

ResolveQuestion questionsQueue[MAX_QUEUE_ENTRIES];
int questionsCount;

void resolveQuestion(int socketDescriptorS_R, char recievedQuestion[], struct sockaddr_in questionSender) {
    Message queryMessage;
    Message responseMessage;
    char recievedQuestionDomainName[MAX_NAME_SIZE];
    QType recievedQuestionQtype = parse_recieved_question(recievedQuestion, recievedQuestionDomainName);
    formatQuestionIntoQuery(recievedQuestionDomainName, recievedQuestionQtype, &queryMessage);
    int appropiate_port = get_appropiate_port(recievedQuestionDomainName);
    if (appropiate_port == -1) {
        buildNotRecognisedResponse(&responseMessage, &queryMessage);
    } else {
        #ifdef DEBUG
        printf("Resolver> Recieved question...\n");
        #endif
        if (checkCache(&queryMessage) == 0) {
            printf("Resolver> Found query in cache rederecting answer to stub...\n");
            buildResponseFromCache(&responseMessage, &queryMessage);
        } else {
            printf("Resolver> Could not find query in cache, sending querry to an appropiate name server...\n");
            int socketDescriptorR_NS;
            struct sockaddr_in foreignServer;
            if ((socketDescriptorR_NS = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("Resolver> Error: Could not create socket to foreign server\n");
                exit(1);
            }
            foreignServer.sin_family = AF_INET;
            foreignServer.sin_addr.s_addr = inet_addr("127.0.0.1");
            foreignServer.sin_port = htons(appropiate_port);
            unsigned int lengthForeignServer = sizeof(foreignServer);
            if (sendto(socketDescriptorR_NS, &queryMessage, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &foreignServer, lengthForeignServer) <= 0) {
                perror("Resolver> Error: Could not send message to foreign server\n");
                exit(1);
            }
            if ((recvfrom(socketDescriptorR_NS, &responseMessage, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &foreignServer, &lengthForeignServer)) < 0) {
                perror("Resolver> Error: Could not recieve message from foreign server\n");
                exit(1);
            }
            if (responseMessage.header.rcode == 0) {
                cacheResponse(&responseMessage);
            }
            close(socketDescriptorR_NS);
        }
    }

    #ifdef DEBUG
    printf("Resolver> Sending back answer...\n");
    #endif

    if (sendto(socketDescriptorS_R, &responseMessage, MAX_MESSAGE_SIZE, 0, (struct sockaddr*) &questionSender, sizeof(struct sockaddr)) <= 0) {
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
    loadNameServerList();
    time_t t;
    srand((unsigned)time(&t));
    struct sockaddr_in resolver;
    struct sockaddr_in stubResolver;
    char recievedQuestion[MAX_QUESTION_SIZE];
    int socketDescriptor;
    pthread_mutex_init(&mutexQuestionsQueue, NULL);
    pthread_cond_init(&condQuestionsQueue, NULL);
    pthread_mutex_init(&mutexCache, NULL);
    for (int i = 0; i < MAX_WORKERS; ++i) {
        if (pthread_create(&workers[i], NULL, &startWorker, NULL) < 0) {
            perror("Resolver> Error: Could not create workers\n");
            exit(1);
        }
    }
    if (pthread_create(&chacheThread, NULL, &cacheMaintenance, NULL) < 0) {
        perror("Resolver> Error: Could not create cache thread\n");
        exit(1);
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

        if ((recvfrom(socketDescriptor, &recievedQuestion, MAX_QUESTION_SIZE, 0, (struct sockaddr*) &stubResolver, &stubResolverSize)) <= 0 ) {
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

    return 0;
}

void loadNameServerList() {
    sList[0].port = 7100;
    strcpy(sList[0].domainName, "ROOT");
    sList[1].port = 7101;
    strcpy(sList[1].domainName, "MRI");
    sList[2].port = 7102;
    strcpy(sList[2].domainName, "CY");
}

int get_appropiate_port(char domainName[MAX_NAME_SIZE]) {
    char domainNameCopy[MAX_NAME_SIZE];
    strcpy(domainNameCopy, domainName);
    int domainNameLen = strlen(domainName);
    for (int i = 0; i < SLIST_SIZE; ++i) {
        for (int j = 0; j < domainNameLen; ++j) {
            domainNameCopy[j] = toupper(domainNameCopy[j]);
        }        
        if (strstr(domainNameCopy, sList[i].domainName)) {
            return sList[i].port;
        }
    }
    return -1;
}


QType parse_recieved_question(char question[MAX_QUESTION_SIZE], char domainName[MAX_NAME_SIZE]) {
    char *tokenizer = strtok(question, " ");
    int index_args = 0;
    while (tokenizer) {
        if (index_args == 0) {
            strcpy(domainName, tokenizer);
        }
        if (index_args == 1) {
            printf("%s", tokenizer);
            if (strcmp(tokenizer, "a") == 0 || strcmp(tokenizer, "A") == 0) {
                return A;
            }
            if (strcmp(tokenizer, "NS") == 0 || strcmp(tokenizer, "ns") == 0) {
                return NS;
            }
            if (strcmp(tokenizer, "CNAME") == 0 || strcmp(tokenizer, "cname") == 0) {
                return CNAME;
            }
        }
        index_args++;
        tokenizer = strtok(NULL, " ");   
    }
    if (index_args == 1) {
        return A;
    }
}

void formatQuestionIntoQuery(char domainName[MAX_NAME_SIZE], QType qType, Message* query) {
    query -> header.id = rand() % 65535 + 1; 
    query -> header.qr = true;
    query -> header.aa = false;
    query -> header.tc = false;
    query -> header.rd = false;
    query -> header.ra = false;
    query -> header.rcode = 0;
    query -> header.anCount = 0;
    strcpy(query -> questionDomain, domainName);
    query -> questionQType = qType;
}

void buildNotRecognisedResponse(Message* response, Message* query) {
    response -> header.aa = false;
    response -> header.id = query -> header.id;
    response -> header.qr = true;
    response -> header.anCount = 0;
    response -> header.ra = false;
    response -> header.rcode = 1;
    response -> header.rd = false;
    response -> header.tc = false;
    strcpy(response -> questionDomain, query -> questionDomain);
    response -> questionQType = query -> questionQType;
}