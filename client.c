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

bool reallocate(int* len, int* buf_len, char** string)
{
    char * backup = *string;
    *string = (char*)realloc(*string,(*buf_len)+MB);
    if(string == NULL)
    {
        fprintf(stderr, "There is no sufficient heap space, Realloc failed\n");
        *string = backup;
        free(*string);
        return false;
    }
    *buf_len = (*buf_len)+MB;
    return true;
}

struct protocol
    {
        uint16_t op;
        uint16_t shift; 
        uint32_t length;
        char message[];
    };

struct protocol* init_protocol(uint16_t op, uint16_t shift, uint32_t length)
{
    /* Allocate the structure protocol */
    struct protocol* protocol = (struct protocol*)calloc(length+8,sizeof(char));

    /* change host byte order to network byte order */
    protocol->op = htons(op);
    protocol->shift = htons(shift);
    protocol->length = htonl(length+8);

    return protocol;
}

int copy_string(struct protocol* protocol, char* message, int n, int offset)
{
    for(int i=0; i<n; i++)
    {
        protocol->message[i] = message[i+offset];
    }
    if(protocol->message[n-1] != '\0')
    {
        printf("offset on\n");
        fflush(stdout);
        protocol->message[n-1] = '\0';
        return 1;
    }
    fflush(stdout);
    return 0;
}

void n_send(int fd, struct protocol* message, int length)
{
    printf("send start\n");
    fflush(stdout);
    char *p = message;
    while(length > 0)
    {
        printf("%d: length\n",length);
        int sent;
        if( (sent = send(fd,p,length+8,0)) <= 0)
        {
            perror("head read error: ");
            fprintf(stdout, "func\n");   
            break;

        }
        senddata += sent-8;
        printf("%d messages are sent to server\n",sent);
        fflush(stdout);
        length = length - sent;
        p += sent;
    }
}

void all_recv(int fd, struct protocol *message, int length)
{
    char *p = message;
    while(length > 0)
    {
        int rec = recv(fd,message,length,0);
        if(rec == -1)
        {
            perror("head receive error: ");
            fprintf(stdout, "func\n");   
            break;
        }
        length = length-rec;
        p += rec;
    }
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
    /* 나중에 2,4 비교하는 것도 넣기 */
    
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
    // addr.sin_family = AF_INET;
    // if(network_order_port(port,&addr.sin_port) == false)
    // {
    //     fprintf(stderr, "Port is out of range(0< <65535\n");
    //     return 0;
    // }
    // if(inet_pton(AF_INET,IP,&addr.sin_addr) != 1)
    // {
    //     fprintf(stderr, "Format of Ip is wrong format\n");
    //     return 0;
    // }
    // memset(&addr.sin_zero,0,sizeof(addr.sin_zero));
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
    printf("Server IP:%s\n",argv[2]);

    /* Connection Success */
    printf("connection success\n");
    /* Allocate the string size of 1MB, if not enough, realloc */
    
    char *string;
    string = (char *)calloc(MB,sizeof(char));
    int input;
    int len = 0;
    int buf_len = MB;
    FILE* pFILE = fopen("5M.txt", "r");
    if(pFILE == NULL)
        printf("fuck, cannot open the file\n");
    /* get character and store to string */
    while((input = fgetc(pFILE)) != EOF)
    {
        len++;
        if(len >= buf_len)
        {
            if(reallocate(&len,&buf_len,&string) == false)
                return 0;
        }
        string[len-1] = input;
    }
    string[len] = '\0';
    len++;
    fclose(pFILE);
    /* Make message with the format of protocol 나중에 protocol 파일로 옮길듯*/
    
    /* Now assum that the len > 10MB */

    int send_len = len;
    int sendlen = send_len;
    int offset = 0;
    int left = 0;
    while(send_len > 0)
    {
        printf("start\n");
        if(send_len > 1000)
            sendlen = 1000;
        else
            sendlen = send_len;
        printf("sendlen: %d\n",sendlen);
        struct protocol* message;
        message = init_protocol((uint16_t)atoi(argv[6]),(uint16_t)atoi(argv[8]),(uint32_t)sendlen);
        left = copy_string(message,string,sendlen,offset);
        n_send(client_server,message,sendlen);
        free(message);
        if(left == 1)
        {
            send_len -= sendlen-1;
            offset += sendlen-1;
            printf("%d is send_len, %d is offset\n", send_len, offset);
        }
        else
        {

            send_len -= sendlen;
            offset += sendlen-1;
        }
        fflush(stdout);
    }

    /* Receive the data from server */
    struct protocol *data = (struct protocol *)calloc(len+8,sizeof(char));
    int recv_len = len+8;
    int revlen = 0;
    while(recv_len > 0)
    {
        int rec = recv(client_server,data,recv_len,0);
        revlen += rec-8;
        // printf("%d letters are received from server\n", rec);
        if(rec == -1)
        {
            perror("head receive error: ");
            fprintf(stdout, "func\n");   
            break;
        }

        recv_len = recv_len-rec;
    }
    printf("revlen is %d\n",revlen);
    fflush(stdout);


    // FILE* pFILE2 = fopen("test.txt", "w");
    // fwrite(&message, sizeof(char), 1000, pFILE2);
    // fclose(pFILE2);
    // FILE* pFILE3 = fopen("test.txt", "r");
    // char string2[1024];
    // int len2 = 0;
    // while(len2 < 500)
    // {
    //     input = fgetc(pFILE3);
    //     len2++;
    //     string2[len2-1] = input;
    // }
    // string2[len] = '\0';

    /* Must free(string) in the end of the file */
    free(string);
    close(client_server);
    return 0;
}