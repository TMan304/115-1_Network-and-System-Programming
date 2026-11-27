#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 5

// Shared Resource
int buffer = 0; 
pthread_mutex_t lock;
pthread_cond_t cond_not_full;  // Signal: Buffer is not full, Producer can work
pthread_cond_t cond_not_empty; // Signal: Buffer is not empty, Consumer can work

// Producer: Adds items to buffer
void *producer(void *arg) {
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&lock);

        // WAIT if buffer is full (Correct way using Cond Var)
        // Note: Use 'while', not 'if', to handle spurious wakeups
        while (buffer == BUFFER_SIZE) {
            printf("[Producer] Buffer Full! Waiting...\n");
            pthread_cond_wait(&cond_not_full, &lock);
        }

        buffer++;
        printf("[Producer] Produced item. Count: %d\n", buffer);

        // Signal consumer that buffer is not empty
        pthread_cond_signal(&cond_not_empty);
        // 我修改的：註解掉解鎖
        // pthread_mutex_unlock(&lock);
        
        sleep(rand() % 2); // Simulate variable work time
    }
    return NULL;
}

// Consumer: Removes items from buffer
void *consumer(void *arg) {
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&lock);

        // WAIT if buffer is empty
        while (buffer == 0) {
            printf("[Consumer] Buffer Empty! Waiting...\n");
            pthread_cond_wait(&cond_not_empty, &lock);
        }

        buffer--;
        printf("[Consumer] Consumed item. Count: %d\n", buffer);

        // Signal producer that buffer is not full
        pthread_cond_signal(&cond_not_full);
        pthread_mutex_unlock(&lock);
        
        sleep(1);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond_not_full, NULL);
    pthread_cond_init(&cond_not_empty, NULL);

    pthread_create(&t1, NULL, producer, NULL);
    pthread_create(&t2, NULL, consumer, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond_not_full);
    pthread_cond_destroy(&cond_not_empty);

    return 0;
}
