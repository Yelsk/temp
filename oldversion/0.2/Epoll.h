/*
 * @Author: GanShuang 
 * @Date: 2020-05-08 21:22:20 
 * @Last Modified by: GanShuang
 * @Last Modified time: 2020-05-08 23:01:49
 */

#pragma once

#include <sys/epoll.h>

class HttpConnection;
class TimerQueue;

class Epoll
{
private:
    int epfd;
    TimerQueue *timerQueue;
public:
    Epoll(/* args */);
    ~Epoll();
    int epoll_add(int fd, int events);
    int epoll_add(HttpConnection *_conn);
    int epoll_mod(HttpConnection *_conn);
    int epoll_del(HttpConnection *_conn);
    int my_epoll_wait(struct epoll_event *events, int max_events, int timeout);
    int getEpollfd() {return epfd;}
    void handleExpired();
};

extern const int MAXEPOLL;