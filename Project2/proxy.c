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
    printf("Number of element: %d\n",i);
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
        *host = header_line[1];
        *port = NULL;
        *path = URL;
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
        *host = header_line[1];
        *port = NULL;
        *path = NULL;
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
    if(end_point == NULL)
    {
        *host = url_host;
        *port = NULL;
        *path = NULL;
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
        *port = NULL;
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
            *path = NULL;
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

    // /* 2번째 / 찾아서 host name 찾기 */
    // char* ptr = strchr(URL, '/');
    // ptr = strchr(ptr+1,'/');
    // /* host name start point */
    // ptr = ptr+1;
    // char *start_point = ptr;
    // char *end_point = strchr(ptr,':');
    // *host = (char*)calloc((end_point-start_point)+1,sizeof(char));
    // strncpy(*host,start_point,end_point-start_point);
    // (*host)[end_point-start_point] = '\0';

    // /* Port start point */
    // start_point = end_point+1;
    // end_point = strchr(start_point, '/');
    // *port = (char*)calloc((end_point-start_point)+1,sizeof(char));
    // strncpy(*port,start_point,end_point-start_point);
    // (*port)[end_point-start_point] = '\0';

    // /* Path start point */
    // start_point = end_point;
    // end_point = strchr(start_point, '\0');
    // *path = (char*)calloc((end_point-start_point)+1,sizeof(char));
    // strncpy(*path,start_point,end_point-start_point);
    // (*path)[end_point-start_point] = '\0';
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

char* make_HTTP_message(char **message)
{
    int l1 = strlen(message[0]);
    int l2 = strlen(message[1]);
    int l3 = strlen(message[2]);
    char* total_message = (char*)malloc(l1+l2+l3+5);
    strncpy(total_message,message[0],l1);
    total_message[l1] = ' ';
    strncpy(&total_message[l1+1],message[1],l2);
    total_message[l1+l2+1] = ' ';
    strncpy(&total_message[l1+l2+2],message[2],l3);
    total_message[l1+l2+l3+2] = '\r';
    total_message[l1+l2+l3+3] = '\n';
    total_message[l1+l2+l3+4] = '\0';
    return total_message;

}

void send_HTTP(int fd, char* message)
{
    /* host 바꿔야 되는데 안 바꿈 안바꿔도 될거 같기는 함*/
    int sd = send(fd,message,strlen(message),0);
    send(fd,"\r\n",2,0);
    printf("sd: %d\n",sd);
}

char* recv_HTTP(int fd)
{
    printf("In Recv function\n");
    char* get = (char*)calloc(10000,sizeof(char));
    int rcv = recv(fd,get,10000,0);
    printf("Rcv: %d\n",rcv);
    return get;
}

void test1(int fd)
{
    printf("@@@@Test 1@@@@\n");

    /* Read until the Only CRLF comes */
    char* buffer = read_request(fd);

    /* Parsing the request to request line, header field */
    char **divide_request = request_parsing(buffer);
    if(divide_request == NULL)
        printf("Invalid request!!!!!\n");

    char **request_line;
    int number_r = 0;
    request_line = message_parsing(divide_request[0], &number_r);

    char **header_line;
    int number_h = 0;
    header_line = message_parsing(divide_request[1], &number_h);

    /* request_line 숫자와 header_line component 숫자가 안맞으면 invalid */
    if((number_r != 3) || (number_h != 2))
        printf("Invalid request!\n");

    /* 연결된 client 정보 ip, port number 출력 */
    /* client 로부터 받은 정보 출력 */
    printf("Get message: %s\n",buffer);

    /* client 로부터 받은 정보가 매우 많을때 나누어서 출력(아직 테스트 안됨) */
    // assert(strcmp(message[0],"GET") == 0);
    // assert(strcmp(message[1],"/nmsl/ee323.txt") == 0);
    // assert(strcmp(message[2],"HTTP/1.0") == 0);

    /* check the error */
    /* 지금은 다 정확한 숫자 만큼 있다고 가정 */
    if(check_request_line(request_line) == -1)
        printf("error case\n");
    if(check_header_field(header_line) == -1)
        printf("Header_field error\n");

    /* Error 404 bad request 보내는 거 */
    
    /* URL을 Parsing host 다 있고 포트 있고, 프로토콜있고 라고 생각*/
    /* 이제 URL을 각 상황에 맞게 분리 및 에러 체크 */
    /* scheme이 이상한 경우만 제외하고 모든 경우 체크 */
    /* www.google.com:/file/help 처럼 :/ 경우도 제외 */
    char* host;
    char* port;
    char* path;
    char* url = request_line[1];
    int valid = URL_parsing(url,header_line,&host,&port,&path);
    if(port == NULL)
        port = "80";

    printf("host: %s\n",host);
    printf("port: %s\n",port);
    printf("host len: %ld\n",strlen(host));
    printf("port len: %ld\n",strlen(port));

    if(path != NULL)
    {

        printf("path: %s\n",path);
        printf("path len: %ld\n",strlen(path));
    }

    /* 받은 URL을 이용해서 server 연결 */
    // int server_fd = connect_server(host,port);
    // /* server에 넣기 (이거 할 때 프록시 서버가 보여야되는거 주의)*/
    // /* HTTP_message 만들기 (\r\n붙여서) */
    // char * send_message = make_HTTP_message(message);
    // send_HTTP(server_fd,send_message);
    // char * get_message;
    // get_message = recv_HTTP(server_fd);
    // printf("%s\n",get_message);

    // /* Send received message to client */
    // send(fd, get_message, strlen(get_message), 0);

    // printf("%%%%Finish%%%%\n");
    // free(buffer);
    /* server 로부터 받은 정보 출력 */
}

int main(int argc, char* argv[])
{

    char *port = argv[1];

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

    if(listen(fd, 10) == -1)
    {
        fprintf(stderr, "listen failed\n");
        return 0;
    }

    printf("host name:%s\n",hostbuffer);
    printf("Port number:%s\n",port);

    /* IP address */
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    addr_size = sizeof(client_addr);
    freeaddrinfo(res);

    int new_fd = accept(fd,(struct sockaddr *)&client_addr,&addr_size);

    /* 연결 완료 */
    /* client 로부터 요청 듣기 */
    

    /* 해당 서버로 요청 보내기 */
    test1(new_fd);
    
    /* 해당 서버로 요청 보내기 */
    close(fd);
    /* message랑 위치 정한거 free 하기 */

    return 0;
}