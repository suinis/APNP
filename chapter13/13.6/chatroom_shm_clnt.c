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
#include <stdbool.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024

int pipefd[2];
bool stopSendProcess = false;
bool stopReadProcess = false;

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// void term_handler(int sig)
// {
//     stopSendProcess = true;
//     stopReadProcess = true;
// }

// void addsig(int sig) 
// {
//     struct sigaction sa;
//     memset(&sa, '\0', sizeof(sa));
//     sa.sa_handler = term_handler;
//     sigfillset(&sa.sa_mask);
//     assert(sigaction(sig, &sa, NULL) != -1);
// }

void sendProcess(int sockfd, int pipefd)
{
    char buf[BUFFER_SIZE];
    struct epoll_event events[MAX_EVENT_NUMBER];
    memset(buf, '\0', BUFFER_SIZE);
    int send_epollfd = epoll_create(5);
    assert(send_epollfd != -1);
    addfd(send_epollfd, pipefd);
    // addsig(SIGTERM);
    while (!stopSendProcess)
    {
        int ret = epoll_wait(send_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }
        else if (ret > 0)
        {
            for(int i = 0; i < ret; ++i)
            {
                int sockfd = events[i].data.fd;
                if (sockfd == pipefd && events[i].events & EPOLLIN)
                {
                    int sig;
                    int recvcnt = recv(pipefd, &sig, 1, 0);
                    if (recvcnt <= 0)
                    {
                        continue;
                    }
                    if (sig == SIGTERM)
                    {
                        stopSendProcess = true;
                        continue;
                    }
                }
            }
        }
        
        fgets(buf, BUFFER_SIZE, stdin);
        printf("send data ('quit' to quit): %s\n", buf);
        if (strncmp(buf, "quit\n", 4) == 0)
        {
            int sig = SIGTERM;
            send(pipefd, &sig, 1, 0);
            break;
        }

        int sendBytes = send(sockfd, buf, strlen(buf), 0);
        if (sendBytes < 0)
        {
            printf("send data failed\n");
            break;
        }
    }
    close(sockfd);
}

void recvProcess(int sockfd, int pipefd)
{
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    // sigadd(SIGTERM);
    while (!stopReadProcess)
    {
        int recvcnt = recv(sockfd, buf, BUFFER_SIZE, 0);
        if (recvcnt == 0)
        {
            printf("server closed the connection\n");
            int sig = SIGTERM;
            send(pipefd, &sig, 1, 0);
            break;
        }
        else if (recvcnt < 0)
        {
            printf("recv data failed\n");
            
            break;
        }
        else
        {
            printf("recv data: %s\n", buf);
        }

        {
            int sig;
            int recvcnt = recv(pipefd, &sig, 1, 0);
            if (recvcnt <= 0)
            {
                continue;
            }
            if (sig == SIGTERM)
            {
                stopReadProcess = true;
                continue;;
            }
        }
    }
    close(sockfd);
}

int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int clntsock = socket(PF_INET, SOCK_STREAM, 0);
    assert(clntsock >= 0);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    int ret = connect(clntsock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0)
    {
        printf("connection failed\n");
        close(clntsock);
        return 1;
    }

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        recvProcess(clntsock, pipefd[1]);
    }
    else
    {
        close(pipefd[1]);
        sendProcess(clntsock, pipefd[0]);
    }

    return 0;
}