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
#include <pthread.h>

/* gcc 编译时需要加上 -lpthread 线程库 */
#define MAX_EVENT_NUMBER 1024
#define BUFSIZE 4

struct fds{
    int epollfd;
    int sockfd;
};

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et){
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    if(enable_et){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/* 这里虽然设置ONESHOT事件，但这之后OS仍然会触发fd上的EPOLLIN事件，且只触发一次 */
void reset_oneshot(int epollfd, int fd){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* worker(void* arg){
    int sockfd = ((struct fds*)arg)->sockfd;
    int epollfd = ((struct fds*)arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);

    char buf[BUFSIZE];
    memset(buf, 0, sizeof(buf));

    while (1)
    {
        int ret = recv(sockfd, buf, BUFSIZE, 0);
        if(ret == 0){
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }else if(ret < 0){
            if(errno == EAGAIN){
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }else{
            printf("get content: %s\n", buf);
            sleep(5); //模拟数据处理过程
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
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
    addfd(epollfd, listenfd, false); //用于监听的socket不能被设置为ONESHOT事件，否则后续的客户连接将无法触发listenfd上的EPOLLIN事件
    
    while (1)
    {
        int count = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(count < 0){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < count; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_in client_addr;
                socklen_t client_addr_sz = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)& client_addr, &client_addr_sz);
                addfd(epollfd, connfd, true); //对每个非监听描述符都注册ONESHOT事件
            }else if(events[i].events & EPOLLIN){
                pthread_t thread;
                struct fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                pthread_create(&thread, NULL, worker, (void*)& fds_for_new_worker);
            }else{
                printf("something else happened!\n");
            }
        }
    }
    
    close(listenfd);
    return 0;
}
