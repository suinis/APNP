#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem {
public:
    sem() {
        if(sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    ~sem() {
        sem_destroy(&m_sem);
    }
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }
    bool post() {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

class locker
{
private:
    pthread_mutex_t m_mutex;
public:
    locker() {
        if(pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    bool Lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool UnLock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
};

class cond
{
private:
    pthread_cond_t m_cond;
    pthread_mutex_t m_mutex;
public:
    cond() {
        if(pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            // 前面cond条件变量init成功，已经分配空间，需要释放
            pthread_cond_destroy(&m_cond);
            throw std::exception();
        }
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }
    bool wait() {
        int ret = 0;
        pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

};

#endif // LOCKER_H