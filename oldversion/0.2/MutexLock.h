/*
 * @Author: GanShuang
 * @Date: 2020-05-05 17:20:24 
 * @Last Modified by: GanShuang
 * @Last Modified time: 2020-05-07 17:05:44
 */

#pragma once

#include <exception>
#include <pthread.h>

class MutexLock
{
public:
    MutexLock(){
        if(pthread_mutex_init(&mutex_, nullptr) != 0){
            throw std::exception();
        }
    };
    ~MutexLock(){
        pthread_mutex_destroy(&mutex_);
    };
    pthread_mutex_t *get(){ return &mutex_; };
    bool lock();
    bool unlock();
private:
    pthread_mutex_t mutex_;
};