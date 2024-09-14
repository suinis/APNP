#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>

#define BUFFER_SIZE 4096
/* 主状态机状态 */
enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER };
/* 从状态机状态 */
enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
/* 服务器处理HTTP请求的结果 */
enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

/* 充当对客户端的响应报文 */
static const char* szret[] = { "I get a correct result\n", "Something wrong\n" };

/* 从状态机，用于解析出一行内容 */
enum LINE_STATUS parse_line(char* buffer, int& checked_index, int& read_index){
    char temp;
    for(; checked_index < read_index; ++checked_index){
        temp = buffer[checked_index];
        if(temp == '\r'){
            if(checked_index + 1 == read_index){
                return LINE_OPEN;
            }
            else if(buffer[checked_index + 1] == '\n'){
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n'){
            if((checked_index > 1) && buffer[checked_index - 1] == '\r'){
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

enum HTTP_CODE parse_requestline(char* temp, enum CHECK_STATE& checkstate){
    char* url = strpbrk(temp, "\t");
    /* 如果请求行中没有空白字符或"\t"字符，则HTTP请求必有问题 */
    if(!url){
        return BAD_REQUEST;
    }
    *url++ = '\0';

    char* method = temp;
    if(strcasecmp(method, "GET") == 0){ /* 仅支持GET方法 */
        printf("The request method is GET\n");
    }
    else{
        return BAD_REQUEST;
    }

    url += strspn(url, " \t");
    char* version = strpbrk(url, " \t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    /* 仅支持HTTP/1.1 */
    if(strcasecmp(version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }
    /* 检查URL是否合法 */
    if(strncasecmp(url, "http://", 7) == 0){
        url += 7;
        url = strchr(url, '/');
    }

    if(!url || url[0] != '/'){
        return BAD_REQUEST;
    }
    printf("The request URL is : %s\n", url);
    /* HTTP请求行处理完毕，状态转移到头部字段的分析 */
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

enum HTTP_CODE parse_headers(char* temp){
    if(temp[0] == '\0'){
        return GET_REQUEST;
    }
    else if(strncasecmp(temp, "Host:", 5) == 0){ /* 处理"Host"头部字段 */
        temp += 5;
        temp += strspn(temp, "\t");
        printf("the request host is : %s\n", temp);
    }else{
        printf("I can not handle this header\n");
    }
    return NO_REQUEST;
}

enum HTTP_CODE parse_content(char* buffer, int& checked_index, enum CHECK_STATE& checkstate, int& read_index, int& start_line){
    enum LINE_STATUS linestatus = LINE_OK;
    enum HTTP_CODE retcode = NO_REQUEST;

    while ((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK)
    {
        char* temp = buffer + start_line;
        start_line = checked_index;

        switch (checkstate)
        {
        case CHECK_STATE_REQUESTLINE:{
            retcode = parse_requestline(temp, checkstate);
            if(retcode == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:{
            retcode = parse_headers(temp);
            if(retcode == BAD_REQUEST){
                return BAD_REQUEST;
            }else if(retcode == GET_REQUEST){
                return GET_REQUEST;
            }
            break;
        }
        default:
            return INTERNAL_ERROR; 
            break;
        } 
    }
    /* 没有读取到完整的一行，则表示还需要继续读取客户端数据才能够进一步分析 */
    if(linestatus == LINE_OPEN){
        return NO_REQUEST;
    }
    else{
        return BAD_REQUEST;
    }
}

int main(int argc, char* argv[]){
    if(argc <= 2){
        printf( "usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen != -1);

    int ret = bind(listenfd, (struct sockaddr*)& servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_addr;
    socklen_t client_addr_sz = sizeof(client_addr);
    int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);

    if(connfd < 0){
        printf("errno is : %d\n", errno);
    }else{
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);

        int data_read = 0;
        int read_index = 0;/* 当前已经读取了多少字节的客户数据 */
        int checked_index = 0;/* 当前已经分析完了多少字节的客户数据 */
        int start_line = 0;/* 行在buffer中的起始位置 */

        enum CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
        while (1)
        {
            data_read = recv(connfd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if(data_read == -1){
                printf("reading failed\n");
                break;
            }
            if(data_read == 0){
                printf("remote client has closed the connection\n");
                break;
            }
            read_index += data_read;
            enum HTTP_CODE result;
            result = parse_content(buffer, checked_index, checkstate, read_index, start_line);

            if(result == NO_REQUEST){
                continue;
            }
            else if(result == GET_REQUEST){
                send(connfd, szret[0], strlen(szret[0]), 0);
                break;
            }else{
                send(connfd, szret[1], sizeof(szret[1]), 0);
                break;
            }
        }
        close(connfd);
    }
    close(listenfd);
    return 0;

}