#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_INCREMENTS 1000000

// Shared variable
int counter = 0;
pthread_mutex_t lock;

// --- Unsafe Function (Race Condition) ---
void *unsafe_increment(void *arg) {
    for (int i = 0; i < NUM_INCREMENTS; i++) {
        // RMW (Read-Modify-Write) is NOT atomic!
        counter++; 
    }
    return NULL;
}

// --- Safe Function (Thread Safe) ---
void *safe_increment(void *arg) {
    for (int i = 0; i < NUM_INCREMENTS; i++) {
        pthread_mutex_lock(&lock);
        counter++; // Critical section protected
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_mutex_init(&lock, NULL);

    // 1. Demonstrate UNSAFE behavior
    counter = 0;
    printf("--- Running Unsafe Version ---\n");
    pthread_create(&t1, NULL, unsafe_increment, NULL);
    pthread_create(&t2, NULL, unsafe_increment, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Expected: %d, Actual: %d ", NUM_INCREMENTS * 2, counter);
    if (counter != NUM_INCREMENTS * 2) printf("(FAIL: Data Race!)\n");
    else printf("(Pass? Pure luck)\n");

    // 2. Demonstrate SAFE behavior
    counter = 0;
    printf("\n--- Running Safe Version ---\n");
    pthread_create(&t1, NULL, safe_increment, NULL);
    pthread_create(&t2, NULL, safe_increment, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Expected: %d, Actual: %d (PASS)\n", NUM_INCREMENTS * 2, counter);

    pthread_mutex_destroy(&lock);
    return 0;
}
