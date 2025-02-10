#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define MAX_EVENT_NUMBER 1024
#define USER_LIMIT 5
#define BUFFER_SIZE 64

struct fds {
    int sockfd;
    char buf[BUFFER_SIZE];
    char* write_buf;
};

int setnonblocking (int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd (int epollfd, int fd, bool oneshot) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET; // 应该改为LT模式？因为后面会清除读事件
    if (oneshot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void reset_oneshot (int epollfd, int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* void* worker (void* arg) {
    int epollfd = ((struct fds*)arg)->epollfd;
    int sockfd = ((struct fds*)arg)->fd;
    
} */

int main (int argc, char* argv[]) {
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    int socketfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(socketfd >= 0);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    int ret = bind(socketfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(socketfd, 5);
    assert(ret != -1);

    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    struct epoll_event events[MAX_EVENT_NUMBER];
    addfd(epollfd, socketfd, false);

    struct fds user_fds[USER_LIMIT];
    int user_count = 0;
    for (int i = 0; i < USER_LIMIT; ++i) {
        user_fds[i].sockfd = -1;
    }

    while (1)
    {
        /* code */
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            printf("epoll failure\n");
            break;
        }
        
        for (int i = 0; i < ret; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == socketfd) {
                struct sockaddr_in clientaddr;
                socklen_t clientaddr_sz = sizeof(clientaddr);
                int connfd = accept(sockfd, (struct sockaddr*)& clientaddr, &clientaddr_sz);
                assert(connfd >= 0);
                addfd(epollfd, connfd, true);
                
                if (user_count < USER_LIMIT) {
                    user_fds[user_count++].sockfd = connfd;
                }
                else {
                    const char* info = "too many users\n";
                    printf(info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
            }
            else if (events[i].events & EPOLLIN) {
                struct fds cur_fds;
                cur_fds.sockfd = sockfd;
                memset(cur_fds.buf, '\0', BUFFER_SIZE);
                int n = recv(sockfd, cur_fds.buf, BUFFER_SIZE - 1, 0);
                if (n < 0) {
                    if (errno != EAGAIN)
                    {
                        printf("recv error\n");
                        close(sockfd);
                        // 遍历user_fds，找到sockfd对应的user_fds，将其清空，user_count自减
                        continue;
                    }
                }
                for (int j = 0; j < USER_LIMIT; ++j) {
                    if(user_fds[i].sockfd == sockfd) {
                        strncpy(user_fds[i].buf, cur_fds.buf, sizeof(cur_fds.buf));
                        continue;
                    }
                    if(user_fds[i].sockfd == -1 || user_fds[i].sockfd == sockfd) continue;
                    user_fds[i].write_buf = cur_fds.buf;
                    // to do: 设置其他连接的写事件 & 清除其他连接的读事件（如何获取指定sockfd的events？内层再for一次events，根据events[i].data.fd设置events[i].events?）
                }
            }
            else if (events[i].events & EPOLLOUT) {
                for (int j = 0; j < USER_LIMIT; ++j) {
                        
                }
                // send(sockfd, )
            }
            else {
                printf("something else happened\n");
            }
        }
    }

    close(socketfd);
    return 0;
}