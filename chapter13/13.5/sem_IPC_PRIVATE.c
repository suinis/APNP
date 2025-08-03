#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

void pv (int sem_id, int op) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    sem_b.sem_flg = SEM_UNDO;
    semop(sem_id, &sem_b, 1);
}

int main (int argc, char* argv[]) {
    int sem_id;
    sem_id = semget(IPC_PRIVATE, 1, 0666 & IPC_CREAT & IPC_EXCL);
    
    union semun sem_union;
    {
        /* data */
        sem_union.val = 1;
    }
    
    semctl(sem_id, 0, SETVAL, sem_union);

    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    else if (pid == 0)
    {
        /* code */
        printf("child try to get binary sem\n");
        pv(sem_id, -1);
        printf("child get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id, 1);
        exit(0);
    }
    else
    {
        /* code */
        printf("parent try to get binary sem\n");
        pv(sem_id, -1);
        printf("parent get the sem and would release it after 5 seconds\n");
        sleep(5);
        pv(sem_id, 1);
    }
    
    waitpid(pid, NULL, 0);
    printf("parent process exit & sem_id : %d\n", sem_id);
    semctl(sem_id, 0, IPC_RMID, sem_union);
    return 0;
}