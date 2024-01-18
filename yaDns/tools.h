#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include "protocol/dns_protocol.h"
#include <time.h>

// for debug purposes
void printMessage(Message* message) {
    printf("Domain Name: %s", message -> questionDomain);
    printf("Question Type: %d", message -> questionQType);
}