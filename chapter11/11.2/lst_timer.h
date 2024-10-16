#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <cstdio>
#include <arpa/inet.h>
#define BUFFER_SIZE 64
class util_timer; /* 前向声明 */

/* 用户数据结构：客户端socket地址、socket文件描述符、读缓存、定时器 */
struct client_data{
    struct sockaddr_in client_addr;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

/* 定时器类 */
class util_timer
{
public:
    /* data */
    time_t expire; //任务的超时时间，这里选用绝对时间
    void (*cb_func)(client_data*); //任务的回调函数
    client_data* user_data;
    util_timer* prev;
    util_timer* next;

public:
    util_timer(): prev(nullptr), next(nullptr) {}
    ~util_timer();
};

/* 升序双向链表，带有头尾节点 */
class sort_timer_lst
{
private:
    /* data */
    util_timer* head;
    util_timer* tail;

private:
    /* 重载的辅助函数，被公有函数add_timer、adjust_timer函数调用：将timer定时器插入到lst_head之后的部分链表中 */
    void add_timer( util_timer* timer, util_timer* lst_head ); 

public:
    sort_timer_lst(): head(nullptr), tail(nullptr) {}
    ~sort_timer_lst(); /* 链表被销毁时，删除其中所有的定时器 */

    void add_timer( util_timer* timer );    /* 添加目标定时器 */
    void adjust_timer( util_timer* timer ); /* 某个定时任务发生变化，调整定时器位置（这里只考虑定时器超时，延长，故往后继链表插入的情况） */
    void delete_timer( util_timer* timer ); /* 删除目标定时器 */
    void tick(); /* SIGALARM信号每次被触发就在信号处理函数/主函数（统一事件源）中调用tick函数，处理到期任务 */
};

void sort_timer_lst::add_timer( util_timer* timer, util_timer* lst_head )
{
    util_timer* prev = lst_head;
    util_timer* tmp = lst_head->next;
    while ( tmp )
    {
        if( timer->expire < tmp->expire )
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = tmp;
            tmp->prev = timer;
            return;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if( !tmp ){
        timer->prev = prev;
        prev->next = timer;
        timer->next = nullptr;
        tail = timer;
    }
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer* tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }   
}

void sort_timer_lst::add_timer( util_timer* timer )
{   
    if( !timer ) return;
    if( !head )
    {
        head = tail = timer;
        return;
    }
    if( timer->expire < head->expire )
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer( timer, head );
}

void sort_timer_lst::adjust_timer( util_timer* timer )
{
    if( !timer ) return;

    /* 如果被调整的定时器在链表尾部/新的超时时间仍然小于后继定时器超时时间，则不变 */
    util_timer* tmp = timer->next;
    if( !tmp | timer->expire < tmp->expire ) return;

    /* 如果该定时器是链表头部，则将该定时器从链表中去除再重新插入链表 */
    if( timer == head )
    {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer( timer, head );
    }

    add_timer( timer, head );
} 

void sort_timer_lst::delete_timer( util_timer* timer )
{
    if( !timer ) return;
    /* 链表中只有一个定时器 */
    if( timer == head && timer == tail )
    {
        delete timer;
        head = tail = nullptr;
        return;
    }
    /* 链表中至少有两个定时器，且timer是头结点 */
    if( timer == head )
    {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    /* 链表中至少有两个定时器，且timer是尾结点 */
    if( timer == tail )
    {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    /* 双端链表删除中间元素 */
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick()
{
    if( !head ) return;

    printf("timer tick()\n");
    time_t cur = time( nullptr );
    util_timer* tmp = head;
    /* 接收到SIGALARM信号，遍历定时器链表，执行所有超时事件 */
    while ( tmp )
    {
        if( tmp->expire > cur) break;
        tmp->cb_func( tmp->user_data );

        head = tmp->next;
        if( head )
        {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head; 
    }
}

#endif