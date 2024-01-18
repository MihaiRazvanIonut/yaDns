#include "../tools.h"
#include "../protocol/dns_protocol.h"

#define PORT 7710
#define MAX_WORKERS 8
#define MAX_QUEUE_ENTRIES 1024
#define SLIST_SIZE 3
#define DEBUG

typedef struct NSInfo {
    int port;
    char domainName[MAX_NAME_SIZE];
} NSInfo;

NSInfo sList [SLIST_SIZE];
void loadNameServerList();
int get_appropiate_port(char domainName[MAX_NAME_SIZE]);
QType parse_recieved_question(char question[MAX_QUESTION_SIZE], char domainName[MAX_NAME_SIZE]);
void formatQuestionIntoQuery(char domainName[MAX_NAME_SIZE], QType QType, Message* query);
char* strlwr(char* str, int len);
int checkCache(Message*);
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
    printf("%s:%d\n", recievedQuestionDomainName, recievedQuestionQtype);
    int appropiate_port = get_appropiate_port(recievedQuestionDomainName);
    printf("%d\n", appropiate_port);
    // error handling in case of not recognised domain name
    formatQuestionIntoQuery(recievedQuestionDomainName, recievedQuestionQtype, &queryMessage);
    #ifdef DEBUG
    printf("Resolver> Recieved question...\n");
    #endif
    printMessage(&queryMessage);
    if (checkCache(&queryMessage) == 0) {

    } else {
        printMessage(&queryMessage);
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
        close(socketDescriptorR_NS);
    }

    /**
     *  todo!()
     *  Based on the answer either return it to the question sender or open new socket to the appropiate server (recursive search)
     */
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
    // handle queryMessage
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
    struct sockaddr_in resolver;
    struct sockaddr_in stubResolver;
    char recievedQuestion[MAX_QUESTION_SIZE];
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
    for (int i = 0; i < SLIST_SIZE; ++i) {
        char* nocase_dn = strlwr(sList[i].domainName, strlen(sList[i].domainName) + 1);
        if (strstr(domainName, nocase_dn)) {
            free(nocase_dn);
            return sList[i].port;
        }
        free(nocase_dn);
    }
    return -1;
}

char* strlwr(char* str, int len) {
    char* lower_str = (char*) malloc(len);
    for (int i = 0; i < len; ++i) {
        lower_str[i] = tolower(str[i]);
    }
    lower_str[len] = '\0';
    return lower_str;
}

QType parse_recieved_question(char question[MAX_QUESTION_SIZE], char domainName[MAX_NAME_SIZE]) {
    char *tokenizer = strtok(question, " ");
    int index_args = 0;
    while (tokenizer) {
        if (index_args == 0) {
            strcpy(domainName, tokenizer);
        }
        if (index_args == 1) {
            if (strcmp(tokenizer, "a") == 0 || strcmp(tokenizer, "A") == 0) {
                return A;
            }
            if (strcmp(tokenizer, "NS") == 0 || strcmp(tokenizer, "ns") == 0) {
                return NS;
            }
        }
        index_args++;
        tokenizer = strtok(NULL, " ");   
    }
    return A;
}

void formatQuestionIntoQuery(char domainName[MAX_NAME_SIZE], QType qType, Message* query) {
    query -> header.qr = true;
    query -> header.aa = false;
    query -> header.tc = false;
    query -> header.rd = false;
    query -> header.ra = false;
    query -> header.rcode = 0;
    query -> header.qdCount = 1;
    query -> header.anCount = 0;
    query -> header.nsCount = 0;
    query -> header.arCount = 0;
    strcpy(query -> questionDomain, domainName);
    query -> questionQType = qType;
    query -> answersList = NULL;
    query -> authorityList = NULL;
    query -> additionalsList = NULL;
    return query;
}
int checkCache(Message*) {
    return -1;
}