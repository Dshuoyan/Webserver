#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
#include<iostream>
#include "sql_connection.h"

using namespace std;
/**
线程池维护管理消费者线程，并提供对外接口。用来往任务队列中添加任务，
消费者(任务)线程负责不断重任务队列中消费任务，通过信号量和互斥锁形成生产者-消费者模型
**/


template <typename T>
class threadpool
{
public:
    /*thread_number:线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(connection_pool* connPool,int thread_number = 8, int max_request = 10000);
    ~threadpool();
    //唯一公共接口用于添加http连接节点
    bool append(T* request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t* m_threads;       //描述线程池的数组，其大小为m_thread_number
    
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //信号量，是否有任务需要处理
    bool threadpool_stop;                //是否结束线程
    std::list<T*> m_workqueue; //请求队列
    connection_pool* m_connPool;  //数据库
};
template <typename T>
threadpool<T>::threadpool(connection_pool *connPool,int thread_number, int max_requests) :m_connPool(connPool), m_thread_number(thread_number), m_max_requests(max_requests), threadpool_stop(false), m_threads(NULL)
{
    if (thread_number <= 0 || max_requests <= 0)//线程池初始化数据有误
    {
        //cout << "线程池初始化数据有误" << endl;
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];//创建线程池数组
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)//成功返回0
        {
            delete[] m_threads;
            throw std::exception();
        }

        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

//析构线程池
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    threadpool_stop = true;
}

//在任务队列末尾添加新的任务
template <typename T>
bool threadpool<T>::append(T* request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//任务队列信号量
    return true;
}

//线程任务函数，由于普通的类成员函数不能作为pthread_creat的参数，特此提供此方法
template <typename T>
void* threadpool<T>::worker(void* arg)
{
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}


template <typename T>
void threadpool<T>::run()
{
    
    while (!threadpool_stop)
    {
        m_queuestat.wait();//信号量阻塞在任务队列上
        m_queuelocker.lock();//上锁
        if (m_workqueue.empty())//broadcast唤醒的，还需要再检查一次
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request)//检查取出的任务是否正常
            continue;
        
        connectionRAII mysqlcon(&request->mysql, m_connPool);

        request->process();//解析http请求
    }
}
#endif
