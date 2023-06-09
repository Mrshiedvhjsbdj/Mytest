#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include "locker.h"
#include <exception>
#include <cstdio>

//线程池类，定义模板类是为了代码的复用，模板参数T是任务类  
template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request); //向请求队列中添加任务

private:
    //线程的数量
    int m_thread_number;
    //线程池数组，大小为m_thread_number
    pthread_t * m_threads;  
    
    //请求队列最多允许的，等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list< T*> m_workqueue;

    //保护请求队列的互斥锁
    locker m_queuelocker;
    
    //信号量用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;

    //线程处理静态函数
    static void * worker(void* arg);

    //线程池从请求队列取数据
    void run();
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false), m_threads(NULL) {

    if ((thread_number <= 0) || (max_requests <=0)) {
        throw std::exception();
    }    

    //创建动态线程池数组
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) {
        throw std::exception();
    }
    
    //创建thread_number个线程，并将它们设置为线程脱离
    for (int i = 0; i < m_thread_number; i++) {
        printf("create the %dth thread\n", i);

        if (pthread_create(m_threads + i, NULL, worker, this) != 0) { //C++中回调函数worker必须为静态函数
            delete [] m_threads;
            throw std::exception();
        }

        //设置线程分离
        if (pthread_detach(m_threads[i])) { //线程分离失败时执行以下代码
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request) {
    //操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void * threadpool<T>::worker(void* arg) {
    threadpool * pool = (threadpool *)arg;
    pool->run();
    return pool;
    //return (void *)pool; ?
}

template<typename T>
void threadpool<T>::run() {
    
    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }

        //执行任务的函数，
        request->process();
    }
}
#endif