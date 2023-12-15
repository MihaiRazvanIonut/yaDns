/**
 *  header containing structures used in the DNS protocol as described in RFC1034, RFC1035 
 *
*/

#define MAX_LABEL_SIZE 64    // octets ( 63 chars + '\0')
#define MAX_NAME_SIZE 256    // octets (255 chars + '\0')
#define MAX_MESSAGE_SIZE 512 // octets 

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
    unsigned short id;
    /**
     * This member is responsible for a bunch of message flags
     * QR 	    Query Response 1 bit: 0 for queries, 1 for responses.
     * OPCODE 	Operation Code 4 bits: Typically always 0, see RFC1035 for details.
     * AA 	    Authoritative Answer 1 bit: Set to 1 if the responding server is authoritative.
     * TC 	    Truncated Message 1 bit: Set to 1 if the message length exceeds 512 bytes.
     * RD 	    Recursion Desired 1 bit: Set by the sender of the request if the server should attempt to resolve the query recursively if it does not have an answer readily available.
     * RA 	    Recursion Available 1 bit: Set by the server to indicate whether or not recursive queries are allowed.
     * Z 	    Reserved 3 bits: Originally reserved for later use, but now used for DNSSEC queries.
     * RCODE 	Response Code 4 bits: Set by the server to indicate the status of the response, i.e. whether or not it was successful or failed
     */
    unsigned short queryInfo;
    unsigned short qdCount;
    unsigned short anCount;
    unsigned short nsCount;
    unsigned short arCount;
} MessageHeader;

typedef struct Message {
    MessageHeader header;
    char* questionsList;
    ResourceRecord* answersList;
    ResourceRecord* authorityList;
    ResourceRecord* additionalsList;
} Message;