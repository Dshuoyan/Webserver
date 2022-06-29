#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
//#include "Log.h"
class util_timer;

//用户数据类，保存在定时器节点中
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer* timer;//主逻辑中通过文件描述符索引client_data数组，从而快速获得链表节点入口，关闭连接
};

//定时器节点类，每一个实例都是链表的一个节点
class util_timer
{
public:
    util_timer():prev(NULL),next(NULL){}

public:
    time_t expire;
    void (*cb_func)(client_data*);//关闭不活跃连接
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};

class sort_timer_lst
{
public:
    sort_timer_lst():head(NULL), tail(NULL) {}
    ~sort_timer_lst();
public:
    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer); 
    void del_timer(util_timer* timer);
    void tick();

private:
    void add_timer(util_timer* timer, util_timer* lst_head);

private:
    util_timer* head;
    util_timer* tail;//尾部
};
#endif

