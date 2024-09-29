#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_EVENT_NUMBER 1024
#define BUFSIZE 4

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et){
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if(enable_et){
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void lt(int epollfd, int num, int listenfd, struct epoll_event* events){
    char buf[BUFSIZE];
    for(int i = 0; i < num; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){ //连接请求
            struct sockaddr_in client_addr;
            socklen_t client_addr_sz = sizeof(client_addr);
            int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);
            addfd(epollfd, connfd, false);
        }
        else if(events[i].events & EPOLLIN){ //不是连接请求，则是由客户端发出的数据接收通告
            printf("event trigger once\n");
            memset(buf, 0, sizeof(buf));
            int ret = recv(sockfd, buf, BUFSIZE, 0);
            if(ret <= 0){  //对方关闭连接
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content : %s\n", ret, buf);
        }else{
            printf("something else happerned!\n");
        }
    }
}

void et(int epollfd, int num, int listenfd, struct epoll_event* events){
    char buf[BUFSIZE];
    for(int i = 0; i < num; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){ //连接请求
            struct sockaddr_in client_addr;
            socklen_t client_addr_sz;
            int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);
            addfd(epollfd, connfd, true);
        }
        else if(events[i].events & EPOLLIN){ //不是连接请求，则是由客户端发出的数据接收通告
            printf("event trigger once\n");
            while (1)
            {
                memset(buf, 0, sizeof(buf));
                int ret = recv(sockfd, buf, BUFSIZE, 0);
                if(ret < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }else if(ret == 0){
                    close(sockfd);
                }else{
                    printf("get %d bytes of content : %s\n", ret, buf);
                }   
            }
        }else{
            printf("something else happerned!\n");
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        exit(0);
    }

    const char* ip = argv[1];
    const int port = atoi(argv[2]);
    int ret = 0;
    struct sockaddr_in serv_addr;
    // memset(&serv_sock, 0, sizeof(serv_sock));
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    // serv_sock.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    ret = bind(listenfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true);
    
    while (1)
    {
        int count = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        lt(epollfd, count, listenfd, events);
        // et(epollfd, count, listenfd, events);
    }
    
    close(listenfd);
    return 0;
}