#include <stdlib.h> // atoi
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <assert.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
} 

void addfd(int epollfd, int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int clntsock = socket(PF_INET, SOCK_STREAM, 0);
    assert(clntsock >= 0);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    int ret = connect(clntsock, (struct sockaddr*)& servaddr, sizeof(servaddr));
    if(ret < 0)
    {
        printf("connection failed\n");
        close(clntsock);
        return 1;
    }

    ret = epoll_create(5);
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];

    
    return 0;
}