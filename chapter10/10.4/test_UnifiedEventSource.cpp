#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

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

void sig_handler(int sig){
    int save_errno = errno; //在处理信号时，errno可能会改变，这里保存errno，用于后面恢复
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags |= SA_RESTART;
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage: %s ip port\n", argv[0]);
        exit(-1);
    }

    const char* ip = argv[1];
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

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    while (!stop_server){
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        /* 当epoll_wait接收到信号，epoll_wait可能会中断，它会返回-1，并设置errno = EINTR，这意味着调用并没有出错，而是由于信号的到来而被中断 */
        if(num < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }
        
        for(int i = 0; i < num; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_in client_addr;
                socklen_t client_addr_sz = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);
                assert(connfd != -1);
                addfd(epollfd, connfd);
            }
            else if(sockfd == pipefd[0] && (events[i].events & EPOLLIN)){
                char signals[1024];
                int signum = recv(sockfd, signals, sizeof(signals), 0);
                if(signum == -1){
                    continue;
                }
                else if(signum == 0){
                    continue;
                }
                else{
                    for(int j = 0; j < signum; ++j){
                        switch (signals[j])
                        {
                        case SIGHUP:
                            /* code */
                            continue;
                        case SIGCHLD:
                            break;
                        case SIGTERM:
                            break;
                        case SIGINT:
                            stop_server = true;
                            break;
                        }
                    }
                }
            }
            else{
                
            }
        }
    }
    printf("close fd\n");
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}