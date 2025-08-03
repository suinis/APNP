#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

int a = 0; 
int b = 0;
pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* another( void* arg ) {
    pthread_mutex_lock(&mutex_b);
    printf("in child thread, got mutex b, waiting for mutex a\n");
    sleep(5);
    ++b;
    pthread_mutex_lock(&mutex_a);
    b += a++;
    pthread_mutex_unlock(&mutex_a);
    pthread_mutex_unlock(&mutex_b);
    pthread_exit(NULL);
}

int main() {
    pthread_t tid;
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);

    pthread_create(&tid, NULL, another, NULL);

    pthread_mutex_lock(&mutex_a);
    printf("in parent thread, got mutex a, waiting for mutex b\n");
    a++;
    sleep(5);
    pthread_mutex_lock(&mutex_b);
    b++;
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&mutex_a);
    pthread_mutex_destroy(&mutex_b);
    return 0;
}