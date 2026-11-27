#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

sem_t baton; // Control the order of execution

void *runner_second(void *arg) {
    printf("[Runner 2] Waiting at the exchange zone...\n");
    
    // Wait until the semaphore value is > 0
    sem_wait(&baton);
    
    printf("[Runner 2] Got the baton! Running to finish line...\n");
    return NULL;
}

void *runner_first(void *arg) {
    printf("[Runner 1] Running...\n");
    sleep(2);
    printf("[Runner 1] Arrived! Passing baton.\n");
    
    // Increment semaphore value (Signal)
    sem_post(&baton);
    
    return NULL;
}

int main() {
    pthread_t t1, t2;

    // Initialize semaphore to 0 (Locked/Red Light)
    sem_init(&baton, 0, 0);

    // Note: We create Runner 2 first to show it really waits
    pthread_create(&t2, NULL, runner_second, NULL);
    sleep(1); 
    pthread_create(&t1, NULL, runner_first, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sem_destroy(&baton);
    return 0;
}
