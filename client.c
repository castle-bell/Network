#include <stdio.h>
#include <stdlib.h> /* to use max function */
#include <string.h> /* to use string cmp, etc function */
#include <ctype.h> /* to use tolower() function */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define MB 1000*1000

int senddata = 0;

int max(int a, int b)
{
    return (a >= b) ? a : b;
}

bool network_order_port(char *port, uint16_t *network_port)
{
    int port_number = atoi(port);
    if( port_number < 0 || port_number >= (1<<16))
    {
        fprintf(stderr,"Port_number is not in range 0 to 65535\n");
        return false;
    }
    uint16_t port_num = (uint16_t)port_number;
    *network_port = htons(port_num);
    return true;
}

bool reallocate(int* occupied, int* capacity, char** string, int need)
{
    while((*capacity - *occupied) <= need)
    {
        char* backup = *string;
        *string = (char*)realloc(*string,(*capacity)+2*MB);
        if(*string == NULL)
        {
            fprintf(stderr, "There is no sufficient heap space, Realloc failed\n");
            *string = backup;
            free(*string);
            return false;
        }
        *capacity += 2*MB;
    }
    return true;
}

struct protocol
    {
        uint16_t op;
        uint16_t shift; 
        uint32_t length;
        char message[];
    };

void init_protocol(struct protocol* protocol, uint16_t op, uint16_t shift, uint32_t length)
{
    /* change host byte order to network byte order */
    protocol->op = op;
    protocol->shift = shift;
    protocol->length = htonl(length);
}

/* copy the string message2(from start to end) -> message 1 */
int copy_string(char* message1, char* message2, int start, int end)
{
    int j = 0;
    for(int i=start; i<=end; i++)
    {
        j++;
        message1[i-start] = message2[i];
    }
    return j;
}

/* Receive all messages length n we want */
int send_all(int fd, char* buffer, size_t length)
{
    int sending = 0;
    while(length>0)
    {
        int sent = send(fd,buffer,length,0);
        if(sent <= 0) return -1;
        buffer += sent;
        length -= sent;
        sending += sent;
    }
    return sending;
}

/* Receive all messages length n we want */
int receive_all(int fd, char* buffer, size_t length)
{
    int receive = 0;
    while(length>0)
    {
        int rcv = recv(fd,buffer,length,0);
        if(rcv <= 0) return false;
        buffer += rcv;
        length -= rcv;
        receive += rcv;
    }
    return receive;
}


/* send message(from start to end) to server, return the length of message sent */
int send_message(int start, int end, char *message,uint16_t op, uint16_t shift, int client_server)
{
    /* make protocol */
    /* protocol length = message_length(end-start+1) + header length(8) + null character(1) */
    int total_length = end-start+10;
    int sent = 0;
    struct protocol *protocol = (struct protocol *)calloc(total_length,sizeof(char));
    init_protocol(protocol,op,shift,total_length);
    copy_string(protocol->message,message,start,end);
    /* set null character at the end of the protocol */
    protocol->message[end-start+1] = '\0';
    /* protocol setting finish */
    /* send to the server */
    char *buffer = (char *)protocol;
    if((sent = send_all(client_server,buffer,total_length)) == -1)
        return -1;
    if(sent != total_length)
        return -1;
    free(protocol);
    return end-start+1;
}

/* Get the total protocol from server and return the message
   copy the message of protocol including '\0\ */
char* receive_message_protocol(int fd, int *received)
{
    int total_length = 0;
    /* copy the string including '\0' */
    int message_length = 0;
    struct protocol* header = (struct protocol*)calloc(8,sizeof(char));

    receive_all(fd,(char *)header,8);
    total_length = ntohl(header->length);
    message_length = total_length-8;

    char *message = (char *)calloc(message_length,sizeof(char));
    receive_all(fd,message,message_length);
    *received = message_length;
    free(header);
    if(message[message_length-1] != '\0')
    {
        return NULL;
    }
    return message;
}

char* receive_message_all(int fd, int sent, int *size)
{
    int message_size = sent;
    int message_occupied = 0;
    int message_start = 0;
    char* message = (char *)calloc(sent,sizeof(char));

    while(message_occupied < sent)
    {
        int received_part = 0;
        char * part = receive_message_protocol(fd,&received_part);
        reallocate(&message_occupied,&message_size,&message,received_part);
        for(int i = 0; i<received_part-1; i++)
        {
            message[i+message_start] = part[i];
        }
        message_start += received_part - 1;
        message_occupied += received_part - 1;
        free(part);
    }
    *size = message_occupied;
    if(message_occupied != sent)
    {
        return NULL;
    }
    return message;
}

int main(int argc, char* argv[])
{
    /* command line 이 맞으면 진행, 아니면 terminate */
    /* If the parameter is less or larger than given format */
    if(argc != 9)
    {
        fprintf(stderr, "Given command line is wrong, Please retry\n");
        return 0;
    }
    /* Compare the parameter with right format */
    if(strncmp("./client",argv[0],max(strlen(argv[0]),8)))
    {
        fprintf(stderr, "File name is wrong, Please retry\n");
        return 0;
    }

    if(strncmp("-h",argv[1],max(strlen(argv[1]),2)))
    {
        fprintf(stderr, "Option is wrong, Please retry\n");
        return 0;
    }
    if(strncmp("-p",argv[3],max(strlen(argv[3]),2)))
    {
        fprintf(stderr, "Option is wrong, Please retry\n");
        return 0;
    }
    if(strncmp("-o",argv[5],max(strlen(argv[5]),2)))
    {
        fprintf(stderr, "Option is wrong, Please retry\n");
        return 0;
    }
    if(strncmp("-s",argv[7],max(strlen(argv[7]),2)))
    {
        fprintf(stderr, "Option is wrong, Please retry\n");
        return 0;
    }

    /* Using Ip address and port number, connect to server */
    char *IP = argv[2];
    char *port = argv[4];
    int IP_address;

    /* Declare the sockaddr in structure */
    struct addrinfo addr;
    struct addrinfo *res;
    memset(&addr,0,sizeof(addr));
    int status = getaddrinfo(IP,port,&addr,&res);
    if(status != 0)
    {
        fprintf(stderr, "Making addr failed\n");
        return 0;
    }
    addr.ai_family = AF_INET;
    addr.ai_socktype = SOCK_STREAM;

    // /* Make socket descriptor */
    int client_server = socket(res->ai_family,res->ai_socktype,0);
    if(client_server == -1)
    {
        fprintf(stderr, "Making socket descriptor failed\n");
        return 0;
    }
    if(connect(client_server,res->ai_addr,res->ai_addrlen) != 0)
    {
        fprintf(stderr, "Connection failed\n");
        return 0;
    }

    /* Connection Success */

    /* Allocate the string size of 1MB, if not enough, realloc */
    
    char *string;
    string = (char *)calloc(MB,sizeof(char));
    int input;
    int len = 0;
    int buf_len = MB;

    /* get character and store to string */
    while((input = fgetc(stdin)) != EOF)
    {
        len++;
        if(len >= buf_len)
        {
            if(reallocate(&len,&buf_len,&string,1) == false)
                return 0;
        }
        string[len-1] = input;
    }
    /* this string is not finished to null character, I'll put the null character when I create the protocol */
    free(res);
    /* Make message with the format of protocol 나중에 protocol 파일로 옮길듯*/
    /* 0~len-1 까지의 string을 send */
    int start = 0;
    char *store = (char *)calloc(2*MB,sizeof(char));
    int store_size = 2*MB;
    int store_occupied = 0;
    int store_start = 0;

    int sent;
    

    while(len>0)
    {
        /* determine the length to send */
        int min_len = (len > MB) ? MB : len;

        /* Send Message */
        if((sent = send_message(start,start+(min_len-1),string,atoi(argv[6]),atoi(argv[8]),client_server)) == -1)
        {
            fprintf(stderr, "Send failed\n");
            return 0;
        }
        /* Get Message */
        int message_size = 0;
        char *message;
        if((message = receive_message_all(client_server,sent,&message_size)) == NULL)
        {
            fprintf(stderr, "Received failed\n");
            return 0;
        }

        /* Copy to store */
        reallocate(&store_occupied,&store_size,&store,message_size);
        for(int i = 0; i<message_size; i++)
        {
            store[i+store_occupied] = message[i];
        }
        store_start += message_size;
        store_occupied += message_size;
        len -= sent;
        start += sent;
        free(message);
    }
    store[store_occupied] = '\0';
    store_occupied++;
    printf("%s",store);
    free(string);
    free(store);
    close(client_server);
    return 0;
}