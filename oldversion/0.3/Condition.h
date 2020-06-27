/*
 * @Author: GanShuang
 * @Date: 2020-05-21 18:59:39
 * @LastEditors: GanShuang
 * @LastEditTime: 2020-05-21 19:00:14
 * @FilePath: /myWebServer-master/oldversion/0.3/Condition.h
 */ 

#pragma once

#include <exception>
#include <pthread.h>

class Condition
{
public:
    Condition();
    ~Condition();
    bool wait(pthread_mutex_t *m_mutex);
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t);
    bool signal();
    bool broadcast();
private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};