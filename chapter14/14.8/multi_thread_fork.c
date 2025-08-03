#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

pthread_mutex_t mutex;

void* child_thread(void* arg) {
    pthread_mutex_lock(&mutex);
    printf("Child thread is running, get mutex!\n");
    sleep(5); // 主要在这句导致的死锁，这里sleep(5)导致主进程在调用fork时，子进程复制的mutex状态是locked，所以就算sleep(5)后unlock该锁，子进程的锁状态也不会变（仍然是fork时的locked状态）
    pthread_mutex_unlock(&mutex);
}

int main() {
    pthread_mutex_init(&mutex, NULL);
    pthread_t tid;

    pthread_create(&tid, NULL, child_thread, NULL);
    sleep(1); // ensure child thread has acquired the mutex

    int pid = fork();
    if ( pid < 0 ) {
        pthread_mutex_destroy(&mutex);
        pthread_join(tid, NULL);
        exit(1);
    } else if ( pid == 0 ) {
        printf("Child process is running, want to get mutex!\n");
        pthread_mutex_lock(&mutex);
        printf("Child process is running, but can't run to here!\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    } else if ( pid > 0 ) {
        wait(NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_join(tid, NULL);
    return 0;
}