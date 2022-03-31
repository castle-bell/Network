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

struct protocol
    {
        uint16_t op;
        uint16_t shift; 
        uint32_t length;
        char message[];
    };

int max(int a, int b)
{
    return (a >= b) ? a : b;
}

bool check_argv(int argc, char **argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "Given command line is wrong, Please retry\n");
        return false;
    }
    /* Compare the parameter with right format */
    
    if(strncmp("./server",argv[0],max(strlen(argv[0]),8)))
    {
        fprintf(stderr, "File name is wrong, Please retry\n");
        return false;
    }
    if(strncmp("-p",argv[1],max(strlen(argv[1]),2)))
    {
        fprintf(stderr, "Option is wrong, Please retry\n");
        return false;
    }
    return true;
}

void init_protocol(struct protocol* protocol, uint16_t op, uint16_t shift, uint32_t length)
{
    /* change host byte order to network byte order */
    protocol->op = op;
    protocol->shift = shift;
    protocol->length = htonl(length);
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
        if(rcv < 0) return -2;
        if(rcv == 0) return -1;
        buffer += rcv;
        length -= rcv;
        receive += rcv;
    }
    return receive;
}

/* Get the total protocol from client and return the protocol */
char* receive_protocol(int fd, int *received)
{
    int total_length = 0;
    /* copy the string including '\0' */
    int message_length = 0;
    struct protocol* header = (struct protocol*)calloc(8,sizeof(char));

    int check;
    if((check = receive_all(fd,(char *)header,8)) == -1)
    {
        /* Connection finished */
        free(header);
        return NULL;
    }
    if(check == -2)
    {
        /* Recv failed */
        free(header);
        return NULL;
    }
    total_length = ntohl(header->length);
    message_length = total_length-8;

    
    char *protocol = (char *)calloc(total_length,sizeof(char));
    char *header_copy = (char *)header;
    /* copy the header */
    for(int i = 0; i < 8; i++){
        protocol[i] = header_copy[i];
    }
    receive_all(fd,&protocol[8],message_length);
    *received = total_length;
    free(header);
    return protocol;
}

/* Check the validity of protocol */
bool check_protocol(struct protocol *protocol)
{
    uint16_t op = protocol->op;
    uint16_t shift = protocol->shift;
    uint32_t length = ntohl(protocol->length);
    /* check op */
    if(op != 0 && op != 1)
        return false;
    /* shift check */
    if((shift<0) || (shift > 65536))
        return false;
    /* length check(10MB) */
    if(length > 10*MB)
        return false;
    /* null character check */
    if(protocol->message[length-9] != '\0')
        return false;
    return true;
}

/* lowering the alphabet and do caesar cypher */
char low_caesar(char letter, uint16_t n, uint16_t option)
{
    /* Do lowering using tolower() */
    int modified;
    modified = tolower(letter);

    /* Do caesar cypher */
    if((modified<'a') || (modified>'z'))
        return modified;
    /* alpha letter */
    int shift = n;
    shift = shift % 26;
    if(option == 1)
    {
        shift = shift*(-1);
    }
    modified += shift;
    if(modified < 'a')
        modified = 'z'-('a'-modified-1);
    if(modified > 'z')
        modified = 'a'+(modified-'z'-1);
    return (char)modified;
}

int main(int argc, char* argv[])
{
    /* check the command line */
    if(check_argv(argc,argv) == false)
    {
        return 0;
    }
    
    char *port = argv[2];

    char hostbuffer[256];
    int hostname;
  
    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
  
    /* Cite the beej's */
    struct addrinfo hints, *res, *p;
    int status;
    memset(&hints, 0 ,sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if((status = getaddrinfo(hostbuffer,port,&hints,&res)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 0;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if((fd ==  -1))
    {
        fprintf(stderr, "Making socket descriptor failed\n");
        return 0;
    }

    if(bind(fd, res->ai_addr, res->ai_addrlen) == -1)
    {
        fprintf(stderr, "Server bind failed\n");
        return 0;
    }

    if(listen(fd, 10 == -1))
    {
        fprintf(stderr, "listen failed\n");
        return 0;
    }

    /* IP address */
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    addr_size = sizeof(client_addr);
    freeaddrinfo(res);

    pid_t pid;
    
    int new_fd;
    while(1)
    {
        new_fd = accept(fd,(struct sockaddr *)&client_addr,&addr_size);
        if(new_fd == -1)
            continue;
        pid = fork();
        if(pid == 0)
            break;
        if(pid < 0)
            fprintf(stderr,"Fork failed\n");
    }
    
    /* connection success */
    if(pid == 0)
    {
        while(1)
        {
            /* read the total amount of protocol */
            int protocol_size = 0;
            struct protocol *protocol = (struct protocol *)receive_protocol(new_fd,&protocol_size);
            if(protocol == NULL)
            {
                /* Connection finished */
                close(new_fd);
                break;
            }

            if(check_protocol(protocol) == false)
            {
                /* reject */
                close(new_fd);
                break;
            }
            uint16_t op = protocol->op;
            uint16_t shift = protocol->shift;
            /* lowering alphabet and caesar cipher */
            for(int i=0; i<protocol_size-8; i++)
            {
                char letter = protocol->message[i];
                protocol->message[i] = low_caesar(letter,shift,op);
            }
            send_all(new_fd,(char *)protocol,protocol_size);
            free(protocol);
        }
        exit(0);
    }
    return 0;
}