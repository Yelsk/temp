/*
 * @Author: GanShuang 
 * @Date: 2020-05-16 20:03:49 
 * @Last Modified by: GanShuang
 * @Last Modified time: 2020-05-16 20:07:31
 */

#include "Condition.h"

Condition::Condition()
{
    if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
}

Condition::~Condition()
{
    pthread_cond_destroy(&m_cond);
}

bool 
Condition::wait(pthread_mutex_t *m_mutex)
{
    int ret = 0;
    //pthread_mutex_lock(&m_mutex);
    ret = pthread_cond_wait(&m_cond, m_mutex);
    //pthread_mutex_unlock(&m_mutex);
    return ret == 0;
}

bool 
Condition::timewait(pthread_mutex_t *m_mutex, struct timespec t)
{
    int ret = 0;
    //pthread_mutex_lock(&m_mutex);
    ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
    //pthread_mutex_unlock(&m_mutex);
    return ret == 0;
}

bool 
Condition::signal()
{
    return pthread_cond_signal(&m_cond) == 0;
}

bool 
Condition::broadcast()
{
    return pthread_cond_broadcast(&m_cond) == 0;
}