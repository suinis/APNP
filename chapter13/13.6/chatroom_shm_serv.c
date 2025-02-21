/* 编译时需要加上链接库： -lrt */

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h> // 共享内存核心函数声明，同下面两个头文件（调用shm_open, shm_unlink,以及涉及文件控制选项以及权限设置）
#include <sys/stat.h> // 文件模式宏（如权限 S_IRUSR、S_IWUSR）
#include <fcntl.h>    // 文件控制选项（如 O_CREAT、O_RDWR）
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <bits/sigaction.h>

#define MAX_EVENT_NUMBER 1024
#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define MAX_SIGNAL_NUMBER 1024
#define FD_LIMIT 65536
#define PROCESS_LIMIT 4194304

struct ClientData
{
    int pipefd[2];
    int connfd;
    int processid;
};

bool stop_child = false;
char *share_mem = 0;
static char* shmname = "/myshm"; // 共享内存对象名命名规则：必须以'/'开头，且中间不能有'/'，长度不能超过NAME_MAX（通常是255）
int sockfd = 0;
int epollfd = 0;
int signal_pipefd[2];
int user_count = 0;
int shmfd = 0;
struct ClientData *users = 0;
int *process_user;

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
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(signal_pipefd[1], &msg, 1, 0);
    errno = save_errno;
}

void child_term_handler(int sig)
{
    stop_child = true;
}

void addsig(int sig, void(*handler)(int), bool restart) 
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if(restart) 
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void del_resource()
{
    close(signal_pipefd[0]);
    close(signal_pipefd[1]);
    close(sockfd);
    close(epollfd);
    shm_unlink(shmname); // 即使进程崩溃，共享内存对象仍会保留，需显式调用 shm_unlink 删除。
    free(users);
    free(process_user);
}

void run_child(int user_count, struct ClientData *users, void *share_mem)
{
    struct epoll_event events[MAX_EVENT_NUMBER];

    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    int connfd = users[user_count].connfd;
    int pipefd = users[user_count].pipefd[1];
    addfd(child_epollfd, connfd);
    addfd(child_epollfd, pipefd);

    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child)
    {
        int num = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        for (int i = 0; i < num; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == connfd && (events[i].events & EPOLLIN))
            { // 接收客户端消息到共享内存
                void *buf = share_mem + user_count * BUFFER_SIZE;
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(connfd, buf, BUFFER_SIZE - 1, 0);

                if (ret < 0 && errno != EAGAIN)
                {
                    stop_child = true;
                }
                else if (ret)
                {
                    stop_child = true;
                }
                else
                {
                    printf("recv data from client%d: %s\n", connfd, (char *)buf);
                    send(pipefd, (char *)&user_count, sizeof(user_count), 0);
                }
            }
            else if (sockfd == pipefd && (events[i].events & EPOLLIN))
            { // 主进程通知本进程有消息到共享内存
                int client = 0;
                int ret = recv(pipefd, (char *)&client, sizeof(client), 0);
                if (ret < 0 && errno != EAGAIN)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
}

int main(int argc, char *argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    bool stop_server = false;
    bool terminate = false;
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    int ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    epollfd = epoll_create(5);
    assert(epollfd != -1);
    struct epoll_event events[MAX_EVENT_NUMBER];

    addfd(epollfd, sockfd);
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, signal_pipefd);
    assert(ret != -1);
    setnonblocking(signal_pipefd[1]);
    addfd(epollfd, signal_pipefd[0]);

    addsig(SIGCHLD, handler, true);
    addsig(SIGTERM, handler, true);
    addsig(SIGINT, handler, true);
    addsig(SIGPIPE, SIG_IGN, true);

    shmfd = shm_open(shmname, O_CREAT | O_RDWR, 0666); // 创建共享内存对象
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE); // 为共享内存分配空间
    assert(ret != -1);
    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0); // 将共享内存对象映射到mmap指定地址
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    users = malloc(sizeof(struct ClientData) * USER_LIMIT);
    process_user = malloc(sizeof(int) * PROCESS_LIMIT);
    for(int i = 0; i < PROCESS_LIMIT; ++i)
    {
        process_user[i] = -1;
    }

    while (!stop_server)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR)
        {
            printf("epoll failure, errno : %d\n", errno);
            break;
        }
        for (int i = 0; i < num; ++i)
        {
            int cur_sockfd = events[i].data.fd;
            if (cur_sockfd == sockfd)
            {
                struct sockaddr_in clientaddr;
                socklen_t clientaddr_len = sizeof(clientaddr);
                int connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientaddr_len);
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT)
                {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                users[user_count].connfd = connfd;                

                pid_t pid = fork();
                if (pid < 0)
                {
                    perror("fork() error!\n");
                    close(connfd);
                    continue;
                }
                else if (pid == 0)
                {
                    close(sockfd);
                    close(epollfd);
                    close(signal_pipefd[0]);
                    close(signal_pipefd[1]);
                    close(users[user_count].pipefd[0]);
                    run_child(user_count, users, share_mem);
                    munmap((void *)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }
                else
                {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].processid = pid;
                    printf("child pid: %d\n", pid);
                    process_user[pid] = user_count;
                    ++user_count;
                }
            }
            else if (cur_sockfd == signal_pipefd[0] && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[MAX_SIGNAL_NUMBER]; // 信号处理为异步执行，管道通信是数据流，且没有数据边界，所以定义一个缓冲区一次性接收所有信号
                ret = recv(signal_pipefd[0], signals, sizeof(signals), 0);
                printf("recv signal from signal_pipefd[0]\n");
                if(ret < 0) 
                {
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                else 
                {
                    for(int j = 0; j < ret; ++j)
                    {
                        switch (signals[j])
                        {
                        case SIGCHLD: //子进程因为客户端断开连接而退出
                        {
                            pid_t pid;
                            int stat;
                            while (pid = waitpid(-1, &stat, WNOHANG) > 0)
                            {
                                int del_user = process_user[pid];
                                process_user[pid] = -1; 
                                if (del_user < 0 || del_user >= USER_LIMIT)
                                {
                                    continue;
                                }
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                close(users[del_user].pipefd[0]);
                                users[del_user] = users[--user_count];
                                process_user[users[del_user].processid] = del_user;
                            }
                            if(terminate && user_count == 0)
                            {
                                stop_server = true;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            printf("kill all the child now\n");
                            if(user_count == 0)
                            {
                                stop_server = true;
                                break;
                            }
                            for(int j = user_count - 1; j >= 0; --j)
                            {
                                int pid = users[j].processid;
                                if (kill(pid, SIGTERM) == -1) {
                                    perror("Failed to send SIGTERM");
                                    continue;
                                }
                                process_user[pid] = -1;
                                epoll_ctl(epollfd, EPOLL_CTL_DEL, users[j].pipefd[0], 0);
                                --user_count;
                            }
                            printf("still have %d child alive\n", user_count);
                            terminate = true; // ???
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) // 子进程的管道有数据输入
            {
                int conn_seq = 0;
                ret = recv(cur_sockfd, (char *)&conn_seq, sizeof(conn_seq), 0); // 注意：接收到的是客户连接的序号
                printf("read data from child across pipe, conn_seq: %d\n", conn_seq);
                if (ret < 0)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < user_count; ++j)
                    {
                        if (users[j].pipefd[0] != cur_sockfd) // 不是自己的管道
                        {
                            printf("send data to child accross pipe\n");
                            send(users[j].pipefd[0], (char *)&conn_seq, sizeof(conn_seq), 0);
                        }
                    }
                }
            }
        }
    }

    // close
    del_resource();
    return 0;
}