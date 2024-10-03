#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENT_SIZE 1024
#define UDP_BUFSIZE 1024
#define TCP_BUFSIZE 512

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void listen_func(int epollfd, int listenfd){
    struct sockaddr_in client_addr;
    socklen_t client_addr_sz = sizeof(client_addr);
    int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);
    assert(connfd != -1);
    addfd(epollfd, connfd);
}

void udp_func(int udpfd){
    fputs("udp_func(): \n", stderr);
    char buf[UDP_BUFSIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_sz = sizeof(client_addr);
    int recvlen = recvfrom(udpfd, buf, UDP_BUFSIZE - 1, 0, (struct sockaddr*) &client_addr, &client_addr_sz);
    assert(recvlen != -1);
    int send_len = sendto(udpfd, buf, recvlen, 0, (struct sockaddr*)& client_addr, client_addr_sz);
    assert(send_len != -1);
}

void tcp_func(int tcpfd){
    fputs("tcp_func(): \n", stderr);
    char buf[TCP_BUFSIZE];
    while (1){
        int recvlen = recv(tcpfd, buf, TCP_BUFSIZE - 1, 0);
        if(recvlen < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            close(tcpfd);
            break;
        }else if(recvlen == 0){
            close(tcpfd);
        }else{
            int sendlen = send(tcpfd, buf, recvlen, 0);
            assert(sendlen != -1);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage: %s ip_address port \n", basename(argv[0]));
        exit(-1);
    }

    struct sockaddr_in serv_addr;
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    /* tcp */
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    int ret = bind(listenfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    /* udp */
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpfd != -1);

    ret = bind(udpfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_SIZE];
    int epollfd = epoll_create(5);
    assert(epollfd > 0);
    addfd(epollfd, listenfd);

    while (1){
        int number = epoll_wait(epollfd, events, MAX_EVENT_SIZE, -1);

        if(number < 0){
            printf("epoll_wait() error\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int fd = events[i].data.fd;
            if(fd == listenfd){
                listen_func(epollfd, listenfd);
            }else if(fd == udpfd){
                udp_func(udpfd);
            }else if(events[i].events & EPOLLIN){
                tcp_func(fd);
            }else{
                printf("something else happened!\n");
            }
        }
    }
    
    close(listenfd);
    return 0;
}