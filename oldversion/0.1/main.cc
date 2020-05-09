#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>
#include "ThreadPool.h"
#include "HttpConnection.h"
#include "MutexLock.h"
#include "Semaphore.h"
#include "Epoll.h"

using namespace std;

const __uint32_t PORT = 8888;
const int LISTENQ = 1024;
const string PATH = "/";
const int EPOLLWAIT_TIME = 10000;
const int TIMER_TIME_OUT = 500;
extern const int MAXEPOLL;

 //设置非阻塞I/O
int
setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    if(old_option == -1) return -1;
    int new_option = old_option | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, new_option) == -1) return -1;
    return old_option;
}

//处理SIGPIPE信号，忽略
void
handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

//向内核事件表中注册fd上的event
void addfd(int epfd, int fd, bool flag)
{
    epoll_event ev;
    ev.data.fd = fd;
    //将epoll设置为Edge Triggered，当epoll_wait检测到其上有事件发生并将此通知应用程序后，应用程序会立即处理该事件
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;  
    if(flag)
    {
        //保证一个socket连接在同一时刻只有一个线程处理
        ev.events = ev.events | EPOLLONESHOT;
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

int
socket_bind_listen(__uint32_t port)
{
    //检查port是否合法
    if(port < 1024 || port > 65535) return -1;

    //创建socket(IPv4 + TCP)，返回监听fd
    int listen_fd = 0;
    if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;

    //消除bind时"Address already"
    int optval = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) return -1;
 
    //设置监听IP和PORT，并与监听fd绑定
    struct sockaddr_in address;
    bzero((char *)&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htons(INADDR_ANY);
    address.sin_port = htons(port);
    if(bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) == -1) return -1;

    //开始监听，最大等待队列列长为LISTENQ
    if(listen(listen_fd, LISTENQ) == -1) return -1;

    //监听fd无效
    if(listen_fd == -1){
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

void
acceptConnection(int listen_fd, const string &path, Epoll *epoll, MutexLock *lock, TimerQueue *timeQueue)
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = 0;
    int accept_fd = 0;
    while(accept_fd  = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len) > 0)
    {
        //设为非阻塞模式
        int ret = setnonblocking(accept_fd);
        if(ret < 0)
        {
            perror("set nonblocking error");
            return;
        }
        // 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
        __uint32_t events = EPOLLIN | EPOLLHUP | EPOLLONESHOT;
        HttpConnection *conn = new HttpConnection(epoll->getEpollfd(), accept_fd, events, epoll);
        //将连接加入epoll事件表
        epoll->epoll_add(conn, EPOLLWAIT_TIME);
        Timer *timer = new Timer(conn, TIMER_TIME_OUT);
        conn->addTimer(timer);
        lock->lock();
        timeQueue->addTimer(timer);
        lock->unlock();
    }
    return;
}

// 分发处理函数
void handle_events(MutexLock *lock, 
                                            Epoll *epoll, 
                                            int listen_fd, 
                                            struct epoll_event* events, 
                                            int events_num, 
                                            const string &path, 
                                            TimerQueue *timerQueue, 
                                            ThreadPool<HttpConnection> *pool)
{
    for(int i = 0; i < events_num; i++)
    {
        // 获取有事件产生的描述符
        HttpConnection *conn = (HttpConnection *)(events[i].data.ptr);
        int fd = conn->getFd();

        // 有事件发生的描述符为监听描述符
        if(fd == listen_fd)
        {
            //cout << "This is listen_fd" << endl;
            acceptConnection(listen_fd, path, epoll, lock, timerQueue);
        }
        else
        {
            // 排除错误事件
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
            {
                printf("error event\n");
                conn->close_coon();
                delete conn;
                continue;
            }
            else if(events[i].events & EPOLLIN)
            {
                // 加入线程池之前将Timer和request分离
                conn->seperateTimer();
                if(conn->myread())
                {
                    // 将请求任务加入到线程池中
                    pool->addjob(conn);
                }
                else
                {
                    conn->close_coon();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                conn->seperateTimer();
                if(conn->mywrite())
                {
                    conn->close_coon();
                }
            }
        }
    }
}

/* 处理逻辑是这样的~
因为(1) 优先队列不支持随机访问
(2) 即使支持，随机删除某节点后破坏了堆的结构，需要重新更新堆结构。
所以对于被置为deleted的时间节点，会延迟到它(1)超时 或 (2)它前面的节点都被删除时，它才会被删除。
一个点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除。
这样做有两个好处：
(1) 第一个好处是不需要遍历优先队列，省时。
(2) 第二个好处是给超时时间一个容忍的时间，就是设定的超时时间是删除的下限(并不是一到超时时间就立即删除)，如果监听的请求在超时后的下一次请求中又一次出现了，
就不用再重新申请requestData节点了，这样可以继续重复利用前面的requestData，减少了一次delete和一次new的时间。
*/

void handle_expired_event(MutexLock *lock, TimerQueue *timerQueue)
{
    lock->lock();
    while (!timerQueue->empty())
    {
        Timer *ptimer_now = timerQueue->top();
        if (ptimer_now->isDeleted())
        {
            timerQueue->pop();
            delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            timerQueue->pop();
            delete ptimer_now;
        }
        else
        {
            break;
        }
    }
    lock->unlock();
}

int main(int argc, char *argv[])
{
    ThreadPool<HttpConnection> *pool = new ThreadPool<HttpConnection>();
    MutexLock *lock = new MutexLock();
    Epoll *epoll = new Epoll();
    TimerQueue *timerQueue = new TimerQueue();
    epoll_event events[1000];
    handle_for_sigpipe();
    int listen_fd = socket_bind_listen(PORT);
    if(listen_fd < 0)
    {
        perror("socket bind failed");
        return 1;
    }
    if(setnonblocking(listen_fd) < 0)
    {
        perror("set nonblocking error");
        return 1;
    }
    __uint32_t event = EPOLLIN | EPOLLET | EPOLLHUP;
    HttpConnection *conn = new HttpConnection(epoll->getEpollfd(), listen_fd, event, epoll);
    epoll->epoll_add(conn, EPOLLWAIT_TIME);
    while(true)
    {
        int events_num = epoll->my_epoll_wait(events, MAXEPOLL, -1);
        if(events_num == 0) continue;
        handle_events(lock, epoll, listen_fd, events, events_num, PATH, timerQueue, pool);
        handle_expired_event(lock, timerQueue);
    }
    return 0;
}