#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>
#include <libgen.h>
#include <string.h>
#include <cassert>
#include <sys/epoll.h>
#include <fcntl.h>
#include <csignal>
#include <11.2/lst_timer.h>

// #define __USE_XOPEN_EXTENDED
#define MAX_EVENT_NUMBER 1024
#define FD_LIMIT 65535
#define TIMESLOT 5

static int pipefd[2];
static int epollfd = 0;
static sort_timer_lst timer_lst; /* lst_timer.h */

int setnonblocking( int sockfd )
{
    int old_option = fcntl(sockfd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, new_option);
    return old_option;
}

void addfd( int epollfd, int sockfd )
{
    epoll_event event;
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
    assert( ret!= -1 );
    setnonblocking(sockfd);
}

void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* ) &msg, 1, 0 );
    errno = save_errno;
}

void addsig( int sig )
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
}

void timer_handler()
{
    /* 定时处理任务，实际就是调用tick函数 */
    timer_lst.tick();
    alarm( TIMESLOT );
}

/* 定时器回调函数：删除非活动连接socket上的注册事件，并关闭之 */
void cb_func( client_data* user_data )
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert( user_data );
    close( user_data->sockfd );
    printf( "close fd %d\n", user_data->sockfd );
}

int main(int argc, char* argv[])
{
    if( argc != 3 )
    {
        printf( "usage: %s ip port \n", argv[0] );
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in serv_addr;
    bzero( &serv_addr, sizeof(serv_addr) );
    serv_addr.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &serv_addr.sin_addr );
    serv_addr.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd != -1 );

    ret = bind( listenfd, ( struct sockaddr* ) &serv_addr, sizeof( serv_addr ) );
    assert( ret!= -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd( epollfd, listenfd );

    ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd );
    assert( ret != -1 );
    setnonblocking( pipefd[1] );
    addfd( epollfd, pipefd[0] );

    addsig( SIGALRM );
    addsig( SIGTERM );
    bool stop_server = false;

    client_data* users = new client_data[ FD_LIMIT ];
    bool timeout = false;
    alarm( TIMESLOT ); 

    while ( !stop_server )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if( number < 0 && errno != EINTR )
        {
            printf( "epoll failure\n" );
            break;
        }

        for ( int i = 0; i < number; ++i )
        {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_addr;
                socklen_t client_addr_sz = sizeof( client_addr );
                int connfd = accept( listenfd, (struct sockaddr*)& client_addr, &client_addr_sz );
                addfd( epollfd, connfd );

                users[connfd].client_addr = client_addr;
                users[connfd].sockfd = connfd;
                /* 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表中 */
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time( nullptr );
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );
            }
            /* 统一事件源，处理信号 */
            else if( sockfd == pipefd[0] && (events[i].events & EPOLLIN) )
            {
                int sig;
                char signals[1024];
                ret = recv( sockfd, signals, sizeof(signals), 0 );
                if( ret == -1 )
                {
                    // handler the error
                    continue;
                }
                else if( ret == 0 ) //管道的写端已经关闭，且管道中没有数据可读
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch ( signals[i] )
                        {
                        case SIGALRM:
                        /* 用timeout变量标记有定时任务需要处理，但不立即处理定时任务，
                        因为定时任务的优先级不是很高，要优先处理其他更重要的任务 */
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                        default:
                            break;
                        }
                    }
                }
            }
            /* 处理客户连接上接收到的数据 */
            else if( events[i].events & EPOLLIN )
            {
                memset( users[sockfd].buf, 0, BUFFER_SIZE);
                ret = recv( sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0 );
                printf( "get %d bytes of client data %s from connfd%d\n", ret, users[sockfd].buf, sockfd );

                util_timer* timer = users[sockfd].timer;
                if( ret < 0 )
                {
                    /* 发生读错误，则关闭连接，并移除其对应的定时器 */
                    if( errno != EAGAIN )
                    {
                        cb_func( &users[sockfd] );
                        if( timer )
                        {
                            timer_lst.delete_timer(timer);
                        }
                    }
                }
                else if( ret == 0 )
                {
                    /* 如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器 */
                    cb_func( &users[sockfd] );
                    if( timer )
                    {
                        timer_lst.delete_timer(timer);
                    }
                }
                else
                {
                    /* 如果某个客户连接上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间 */
                    if( timer )
                    {
                        time_t cur = time( nullptr );
                        timer->expire = cur + 3 * TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                }
            }
            /* 处理其他事件 */
            else
            {

            }
        }
        /* 最后处理定时事件，因为I/O事件有更高的优先级。
        当然，这样做会导致定时任务不能精确按照预期时间执行 */
        if( timeout )
        {
            timer_handler();
            timeout = false;
        }
    }
    
    close( listenfd );
    close( pipefd[0] );
    close( pipefd[1] );
    delete[] users;
    return 0;
}