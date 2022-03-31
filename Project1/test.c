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



struct protocol
    {
        uint16_t op;
        uint16_t shift; 
        uint32_t length;
        char message[];
    };



int main(int argc, char* argv[])
{
    /* command line 이 맞으면 진행, 아니면 terminate */
    /* If the parameter is less or larger than given format */

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

    struct protocol* protocol = (struct protocol*)calloc(12,sizeof(char));
    protocol->op = 0;
    protocol->shift = 5;
    protocol->length = htonl(12);
    protocol->message[0] = 'a';
    protocol->message[1] = 'a';
    protocol->message[2] = 'a';
    protocol->message[3] = 'a';

    int sent = 0;
    int sent2 = 0;
    sent = send(client_server,&protocol,11,0);
    sent2 = send(client_server,&protocol,11,0);
    printf("%d is sent %d is sent2\n",sent,sent2);
    return 0;
}
