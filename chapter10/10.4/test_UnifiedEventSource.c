#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENT_NUMBER 1024

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
    setnonblocking(fd);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage: %s ip port\n", argv[0]);
        exit(-1);
    }

    const* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in serv_addr;

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    int ret = bind(listenfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    int epollfd = epoll_create(5);
    addfd(epollfd, listenfd);
    struct epoll_event events[MAX_EVENT_NUMBER];
    while (1){
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if()
    }
    

    return 0;
}