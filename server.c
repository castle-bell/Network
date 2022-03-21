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

int main(int argc, char* argv[])
{
    char *port = argv[2];

    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
  
    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
  
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
  
    // To convert an Internet network
    // address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr*)
                           host_entry->h_addr_list[0]));
  
    printf("Hostname: %s\n", hostbuffer);
    printf("Host IP: %s", IPbuffer);

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


    int new_fd;
    while(1)
    {
        new_fd = accept(fd,(struct sockaddr *)&client_addr,&addr_size);
        if(new_fd == -1)
            continue;
        printf("success\n");

        /* connection success */

    }

    /* Connection Success */

    return 0;
}