#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define MAX_EVENT_NUMBER 1024
#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65536

struct ClientData {
    int pipefd[2];
    int connfd;
};

int signal_pipefd[2];
int user_count = 0;
int shmfd = 0;
char* share_mem = 0;
struct ClientData* users = 0;


int setnonblocking (int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd (int epollfd, int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

int main (int argc, char* argv[]) {
    const char* ip = argv[1];
    const int port = atoi(argv[2]);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    int ret = bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    struct epoll_event events[MAX_EVENT_NUMBER];

    addfd(epollfd, sockfd);
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, signal_pipefd);
    assert(ret != -1);
    setnonblocking(signal_pipefd[1]);
    addfd(epollfd, signal_pipefd[0]);

    shmfd = shm_open(NULL, O_CREAT | O_RDWR, 0666);   // 创建共享内存对象
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE); // 为共享内存分配空间
    assert(ret != -1);
    share_mem = mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0); // 将共享内存对象映射到mmap指定地址
    assert(share_mem != MAP_FAILED);

    users = malloc(sizeof(struct ClientData) * USER_LIMIT);

    while (1)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0) {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < num; ++i)
        {
            int cur_sockfd = events[i].data.fd;
            if (cur_sockfd == sockfd) {
                struct sockaddr_in clientaddr;
                socklen_t clientaddr_len = sizeof(clientaddr);
                int connfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientaddr_len);
                if (connfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                users[user_count].connfd = connfd;
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);

                pid_t pid = fork();
                if (pid < 0) {
                    close(connfd);
                    continue;
                }
                else if (pid == 0) {
                    close(sockfd);
                    close(epollfd);
                    close(signal_pipefd[0]);
                    close(signal_pipefd[1]);
                    close(users[user_count].pipefd[0]);
                    addfd(epollfd, connfd);
                    
                }
                else {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                }
            } 
            else if () {

            }
            else if () {

            }
        }
        
    }

    //close
    return 0; 
}