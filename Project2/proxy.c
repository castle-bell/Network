#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>

int max_str(char* a, char* b)
{
    int length_a = strlen(a);
    int length_b = strlen(b);
    return length_a > length_b ? length_a:length_b;
}

/* Find the end_point of host name using : or / 
   if there is no : and / then return the point where \0 locates */
char* find_end_point(char* start_point)
{
    char* p = strchr(start_point,':');
    char* s = strchr(start_point,'/');
    if(p == NULL && s == NULL)
        return strchr(start_point,'\0');
    if(p == NULL)
        return s;
    if(s == NULL)
        return p;
    return (p < s) ? p : s;
}

char* alloc_and_copy(char *message)
{
    int length = strlen(message);
    char* copy = (char*)malloc(length+1);
    strncpy(copy,message,length);
    copy[length] = '\0';
    return copy;
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


char *read_request(int fd)
{
    int read = 0;
    int capacity = 100;
    char *buffer = (char *)calloc(100,sizeof(char));
    while(1)
    {
        if(capacity - read <= 50)
        {
            buffer = realloc(buffer, capacity+50);
            capacity += 50;
        }
        read += recv(fd,&buffer[read],40,0);
        if(read >= 4)
        {
            /* last element */
            bool a1 = (buffer[read-1] == '\n');
            bool a2 = (buffer[read-2] == '\r');
            bool a3 = (buffer[read-3] == '\n');
            bool a4 = (buffer[read-4] == '\r');
            if(a1&&a2&&a3&&a4)
            {
                buffer[read-1] = '\0';
                buffer[read-2] = '\0';
                return buffer;
            }
        }
    }
}

char **request_parsing(char *request_complex)
{
    char ** request = (char**)calloc(5,sizeof(char*));
    char * copy = request_complex;

    copy = strtok(copy,"\n");
    int i = 0;
    while((copy != NULL) && (i <5))
    {
        request[i] = copy;
        copy = strtok(NULL,"\n");
        i++;
    }

    /* 이때 만약 request 갯수가 2개가 아니면 invalid request */
    if(i != 2)
    {
        return NULL;
    }

    /* \n 기준으로 나누고 \r은 \0로 바꾸어서 보낸다. */
    for(int j = 0;j<i;j++)
    {
        int length = strlen(request[j]);
        request[j][length-1] = '\0';
    }

    return request;
}

/* Get the string and return parsed message */
char **message_parsing(char *message, int* number)
{
    char ** parsed = (char **)calloc(10,sizeof(char*));
    char * copy = message;

    copy = strtok(copy," ");
    int i = 0;
    while((copy != NULL) && (i<10))
    {
        parsed[i] = copy;
        copy = strtok(NULL," ");
        i++;
    }
    *number = i;
    return  parsed;
}

/* Return -1 if request_line is invalid, else return 0 */
int check_request_line(char *request_line[])
{
    char *cmd = request_line[0];
    char *version = request_line[2];
    if(strncmp(cmd,"GET",max_str(cmd,"GET")) != 0)
        return -1;
    if(strncmp(version, "HTTP/1.0",max_str(version,"HTTP/1.0")) != 0)
        return -1;
    return 0;
}

/* Return -1 if header_field is invalid, else return 0 */
int check_header_field(char *header_field[])
{
    char *name = header_field[0];
    char *value = header_field[1];
    if(strncmp(name,"Host:", max_str(name,"Host:")) == 0)
    {
        /* Name = Host: */
        struct hostent *host;
        host = gethostbyname(value);
        if(host == NULL)
            return -1;
    }
    return 0;
}

/* Return -1, parsing fail, if host is omitted return 1 else, return 0,
 parsed URL through parameter, it consists of host, port, path in order */
int URL_parsing(char* URL, char **header_line, char **host, char **port, char **path)
{
    printf("URL: %s\n",URL);
    /* 첫번째 / 찾아서 그 /의 위치가 0이면 hostname 생략 후 path 부터 시작 */
    char* slash = strchr(URL, '/');
    if(slash == &URL[0])
    {
        *host = alloc_and_copy(header_line[1]);
        *port = alloc_and_copy("80");
        *path = alloc_and_copy(URL);
        return 1;
    }
    char* host_name = &URL[0];
    char* colon = strchr(URL, ':');
    if(slash != NULL && colon != NULL)
    {
        char* slash2 = strchr(slash+1,'/');
        /* :// 가 있는지 체크해서 scheme이 있는지 여부를 찾고 그 다음을 
           host_name의 start point로 세팅 scheme이 엉망진창인 경우는 제외*/
        if((slash2 != NULL) && (colon+1 == slash) && (colon+2 == slash2))
        {
            host_name = slash2+1;
        }
    }
    /* a;lkdj:// 로 끝나는 경우 */
    if(*host_name == '\0')
    {
        *host = alloc_and_copy(header_line[1]);
        *port = alloc_and_copy("80");
        *path = alloc_and_copy("");
        return 1;
    }

    /* make host */
    char *end_point;
    char *start_point;
    char *url_host;
    end_point = find_end_point(host_name);
    url_host = (char*)calloc(end_point-host_name+1,sizeof(char));
    strncpy(url_host,host_name,end_point-host_name);
    url_host[end_point-host_name] = '\0';

    /* Port 도 생략되고 / 도 없는 경우(path가 없는경우) */
    if(*end_point == '\0')
    {
        *host = url_host;
        *port = alloc_and_copy("80");
        *path = alloc_and_copy("");
        return 0;
    }

    /* Port가 생략된 경우 */
    if(*end_point == '/')
    {
        start_point = end_point;
        end_point = strchr(end_point,'\0');
        char* url_path = (char*)calloc(end_point-start_point+1,sizeof(char));
        strncpy(url_path,start_point,end_point-start_point);
        url_path[end_point-start_point] = '\0';
        
        *host = url_host;
        *port = alloc_and_copy("80");
        *path = url_path;
        return 0;
    }

    if(*end_point == ':')
    {
        start_point = end_point+1;
        end_point = strchr(end_point,'/');
        if(end_point == NULL)
        {
            /* path가 없는 경우 */
            end_point = strchr(URL,'\0');
            char* url_port = (char*)calloc(end_point-start_point+1,sizeof(char));
            strncpy(url_port,start_point,end_point-start_point);
            url_port[end_point-start_point] = '\0';

            *host = url_host;
            *port = url_port;
            *path = alloc_and_copy("");
        }
        else
        {
            end_point = strchr(end_point,'/');
            char* url_port = (char*)calloc(end_point-start_point+1,sizeof(char));
            strncpy(url_port,start_point,end_point-start_point);
            url_port[end_point-start_point] = '\0';

            start_point = end_point;
            end_point = strchr(end_point,'\0');
            char* url_path = (char*)calloc(end_point-start_point+1,sizeof(char));
            strncpy(url_path,start_point,end_point-start_point);
            url_path[end_point-start_point] = '\0';

            *host = url_host;
            *port = url_port;
            *path = url_path;
        }
        return 0;
    }
    return -1;
}

int check_URL(char *host, char* header_host)
{
    /* check valid host */
    if(gethostbyname(host) == NULL)
        return -1;
    if(strcmp(host,header_host) != 0)
        return -1;
    return 0;
}

int connect_server(char *host, char *port)
{
    /* Declare the sockaddr in structure */
    struct addrinfo addr;
    struct addrinfo *res;
    memset(&addr,0,sizeof(addr));
    int status = getaddrinfo(host,port,&addr,&res);
    if(status != 0)
    {
        fprintf(stderr, "Making addr failed\n");
        return 0;
    }
    addr.ai_family = AF_INET;
    addr.ai_socktype = SOCK_STREAM;

    // /* Make socket descriptor */
    int fd = socket(res->ai_family,res->ai_socktype,0);
    if(fd == -1)
    {
        fprintf(stderr, "Making socket descriptor failed\n");
        return 0;
    }
    if(connect(fd,res->ai_addr,res->ai_addrlen) != 0)
    {
        fprintf(stderr, "Connection failed\n");
        return 0;
    }
    return fd;
}

/* 두 string 다 \0로 끝나야만 함. */
char* str_concat(char* a, char *b)
{
    int length_a = strlen(a);
    int length_b = strlen(b);
    char *concat = (char*)malloc(length_a+length_b+1);
    strncpy(concat,a,length_a);
    strncpy(&concat[length_a],b,length_b);
    concat[length_a+length_b] = '\0';
    return concat;
}

char* make_HTTP_message(char **message, char* URL)
{
    int l1 = strlen(message[0]);
    int l2 = strlen(URL);
    int l3 = strlen(message[2]);
    char* total_message = (char*)malloc(l1+l2+l3+5);
    strncpy(total_message,message[0],l1);
    total_message[l1] = ' ';
    strncpy(&total_message[l1+1],URL,l2);
    total_message[l1+l2+1] = ' ';
    strncpy(&total_message[l1+l2+2],message[2],l3);
    total_message[l1+l2+l3+2] = '\r';
    total_message[l1+l2+l3+3] = '\n';
    total_message[l1+l2+l3+4] = '\0';
    return total_message;
}

char* make_entire_URL(char* host, char* port, char* path)
{
    int length = strlen(host) + strlen(port) + strlen(path) + 9;
    int ptr = 0;
    char *URL = (char*)malloc(length);
    
    strncpy(URL,"http://", 7);
    ptr = 7;
    strncpy(&URL[ptr],host,strlen(host));
    ptr += strlen(host);
    strncpy(&URL[ptr],":",1);
    ptr += 1;
    strncpy(&URL[ptr],port,strlen(port));
    ptr += strlen(port);
    strncpy(&URL[ptr],path,strlen(path));
    ptr += strlen(path);
    URL[ptr] = '\0';
    return URL;
}

void Bad_request(int fd)
{
    char *bad ="HTTP/1.0 400 Bad Request";
    send(fd,bad,strlen(bad),0);
    send(fd,"\r\n\r\n",4,0);
    close(fd);
}

void send_HTTP(int fd, char* message, char** header_line)
{
    /* host 바꿔야 되는데 안 바꿈 안바꿔도 될거 같기는 함*/
    int sd = send(fd,message,strlen(message),0);
    send(fd,header_line[0],strlen(header_line[0]),0);
    send(fd," ",strlen(" "),0);
    send(fd,header_line[1],strlen(header_line[1]),0);
    send(fd,"\r\n",2,0);
    send(fd,"\r\n",2,0);
}

char** get_black_list(int* list_number)
{
    /* check the stdin that is ready to read */
    /* make stding non blocked */
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    int ready = poll(fds,1,0);

    /* stdin에 black_list가 들어오지 않았을 때 */
    if(ready == 0)
        return NULL;
    
    if(ready == -1)
    {
        fprintf(stderr,"Poll Error occureed\n");
        return NULL;
    }

    /* stdin에 black_list가 들어왔을 때 */
    char ** black_list = (char**)calloc(1000,sizeof(char*));
    int capacity = 1000;
    char* copy;

    while(1)
    {
        char* black = (char*)calloc(100,sizeof(char));
        copy = black;
        black = fgets(black,100,stdin);
        if(black == NULL)
        {
            free(copy);
            break;
        }
        /* remove white space character */
        int len = strlen(black);
        if((black[len-1] == '\r') || (black[len-1] == '\n'))
            black[len-1] = '\0';
        if((black[len-2] == '\r') || (black[len-2] == '\n'))
            black[len-2] = '\0'; 
        black_list[*list_number] = black;
        *list_number += 1;
    }
    return black_list;
}

void check_blacklist(char** black_list, int number, char** host, char** port, char** path)
{
    for(int i=0;i<number;i++)
    {
        if(strcmp(*host,&black_list[i][7]) == 0)
        {
            free((*host));
            free((*port));
            free((*path));
            *host = "warning.or.kr";
            *port = "80";
            *path = "/";
            return;
        }
    }
    return;
}

char* recv_HTTP(int fd, int *msg_length)
{
    int rcv = 1;
    int length = 0;
    int size = 10000;
    char* get = (char*)calloc(size,sizeof(char));
    while(rcv > 0)
    {
        if((size-length) < 1000)
        {
            /* Handle error case later */
            get = realloc(get, size+2000);
            size += 2000;
        }
        rcv = recv(fd,&get[length],size-length,0);
        length += rcv;
    }
    *msg_length = length;
    return get;
}

int main(int argc, char* argv[])
{

    char *get_port = argv[1];
    char ** black_list;
    int list_number = 0;

    /* Get black list */
    black_list = get_black_list(&list_number);
  
    /* Cite the beej's */
    struct addrinfo hints, *res, *p;
    int status;
    memset(&hints, 0 ,sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if((status = getaddrinfo(NULL,get_port,&hints,&res)) != 0)
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

    if(listen(fd, 10) == -1)
    {
        fprintf(stderr, "listen failed\n");
        return 0;
    }

    /* IP address */
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    addr_size = sizeof(client_addr);
    freeaddrinfo(res);


    /* Multi client process */
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
        if(pid > 0)
            close(new_fd);
        if(pid < 0)
            fprintf(stderr,"Fork failed\n");
    }

    if(pid == 0)
    {
        /* Read request from client */
        char* buffer = read_request(new_fd);

        /* Parsing the request to request line, header field */
        char **divide_request = request_parsing(buffer);
        if(divide_request == NULL)
        {
            Bad_request(new_fd);
            free(buffer);
            close(new_fd);
            exit(-1);
        }

        char **request_line;
        int number_r = 0;
        request_line = message_parsing(divide_request[0], &number_r);

        char **header_line;
        int number_h = 0;
        header_line = message_parsing(divide_request[1], &number_h);


        if((number_r != 3) || (number_h != 2))
        {
            Bad_request(new_fd);
            free(buffer);
            close(new_fd);
            exit(-1);
        }


        if(check_request_line(request_line) == -1)
        {
            Bad_request(new_fd);
            free(buffer);
            close(new_fd);
            exit(-1);
        }
        if(check_header_field(header_line) == -1)
        {
            Bad_request(new_fd);
            free(buffer);
            close(new_fd);
            exit(-1);
        }

        char* host;
        char* port;
        char* path;
        char* url = request_line[1];
        URL_parsing(url,header_line,&host,&port,&path);

        /* check validity of URL */
        int validity;
        validity = check_URL(host,header_line[1]);
        if(validity == -1)
        {
            Bad_request(new_fd);
            free(buffer);
            free(host);
            free(port);
            free(path);
            close(new_fd);
            exit(-1);
        }

        /* check black list and change the host */
        /* 여기 path가 NULL인 경우, port가 "80"인 경우도 들어가면 바로 에러 뜰듯 */
        check_blacklist(black_list,list_number,&host,&port,&path);

        // /* 받은 URL을 이용해서 server 연결 */
        int server_fd = connect_server(host,port);
        /* server에 넣기 (이거 할 때 프록시 서버가 보여야되는거 주의)*/

        /* Make entire URL */
        char *URL = make_entire_URL(host,port,path);

        /* HTTP_message 만들기 (\r\n붙여서) */
        char * send_message = make_HTTP_message(request_line, URL);
        send_HTTP(server_fd,send_message, header_line);

        int length;
        char * get_message;
        get_message = recv_HTTP(server_fd, &length);

        close(server_fd);

        /* Send received message to client */
        int n = send_all(new_fd, get_message, length);
        // send(new_fd,'\0',1,0);

        /* Free all dynamically allocated memory */
        free(buffer);
        free(host);
        free(port);
        free(path);
        free(get_message);
        free(URL);
        close(new_fd);

        /* message랑 위치 정한거 free 하기 */
    }
    return 0;
}