#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define handle_error_en(en, msg) \ 
    do {errno = en; perror(msg); exit(EXIT_FAILURE);} while (0)


void* thread_func(void* arg) {
    sigset_t* set = (sigset_t*)arg;
    int sig, ret;
    for (; ;) {
        ret = sigwait(set, &sig);
        if (ret != 0) {
            handle_error_en(ret, "sigwait");
        }
        printf("Thread received signal: %d\n", sig);
    }
}

int main() {
    pthread_t tid;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);

    int ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (ret != 0) {
        handle_error_en(ret, "pthread_sigmask");
    }

    ret = pthread_create(&tid, NULL, thread_func, (void*)&set);
    if (ret != 0) {
        handle_error_en(ret, "pthread_create");
    }
    pause();
}