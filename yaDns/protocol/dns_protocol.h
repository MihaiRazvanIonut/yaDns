/**
 *  header containing structures used in the DNS protocol as described in RFC1034, RFC1035 
 *
*/
#ifndef DNS_PROTOCOL_HH
#define DNS_PROTOCOL_HH

#include <stdbool.h>
#define MAX_LABEL_SIZE 64       // octets ( 63 chars + '\0')
#define MAX_NAME_SIZE 256       // octets (255 chars + '\0')
#define MAX_MESSAGE_SIZE 512    // octets 
#define MAX_QUESTION_SIZE 268   // domain name (255 octets) + " " +
                                // qType (2 byte integer)   + " " +
                                // qClass (2 byte integer)  + "\0" 

/**
 *  TTL      - 32 bit signed integer
 *  RDlength - unsigned 16 bit integer
 *  RData    - variable length string of octets
*/

typedef enum QType {
    A        = 1,   //  host adress
    NS       = 2,   //  authoritative name server
    CNAME    = 5,   //  the canonical name for an alias
    SOA      = 6,   //  marks the start of a zone of authority
    PTR      = 12,  //  domain name pointer
    MX       = 15,  //  mail exchange
    TXT      = 16,  //  text strings
    AXFR     = 252, //  request for a transfer of an entire zone
    ALLTYPES = 255, //  request for all records
} QType;

typedef enum QClass {
    IN  = 1,    // internet
    ALLCLASSES = 255   // any class
} QClass;

typedef struct ResourceRecord {
    char domainName[MAX_NAME_SIZE];
    QType rrType;
    QClass rrClass; // Class always set to 1
    int timeToLive;
    char* rData;
    unsigned short int rdLength;
} ResourceRecord;

typedef struct QuestionInfo {
    int questionCategory;
    char domainName[MAX_NAME_SIZE];
    enum QType qType;
    enum QClass qClass;
} QuestionInfo;

typedef struct MessageHeader {
    bool qr;
    bool aa;
    bool tc;
    bool rd;
    bool ra;
    unsigned short rcode;
    unsigned short qdCount;
    unsigned short anCount;
    unsigned short nsCount;
    unsigned short arCount;
} MessageHeader;

typedef struct Message {
    MessageHeader header;
    char questionDomain[MAX_NAME_SIZE];
    QType questionQType;
    ResourceRecord* answersList;
    ResourceRecord* authorityList;
    ResourceRecord* additionalsList;
} Message;

#endif