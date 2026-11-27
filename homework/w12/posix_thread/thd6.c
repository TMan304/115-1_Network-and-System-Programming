#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_THREADS 4

// ---------------------------------------------------------
// Scenario 1: THE TRAP (Wrong Way)
// Receiving a pointer to 'i', which Main is changing rapidly!
// ---------------------------------------------------------
void *print_id_wrong(void *arg) {
    int *ptr = (int *)arg; 
    // Sleep to simulate thread startup delay (exposing the race condition)
    usleep(1000); 
    int my_id = *ptr; // Dereference: By now, 'i' in main might be 4!
    printf("[Wrong] I think I am Thread %d (Address: %p)\n", my_id, ptr);
    return NULL;
}

// ---------------------------------------------------------
// Scenario 2: FIX A - PASS BY VALUE (Quick Fix)
// Casting the integer value directly into the void pointer
// ---------------------------------------------------------
void *print_id_value(void *arg) {
    // Cast the pointer value back to long, then to int
    int my_id = (int)(long)arg; 
    printf("[Fix A] I am Thread %d (Passed by Value)\n", my_id);
    return NULL;
}

// ---------------------------------------------------------
// Scenario 3: FIX B - PASS BY MALLOC (Robust Fix)
// Allocating separate memory for each thread
// ---------------------------------------------------------
void *print_id_malloc(void *arg) {
    int *ptr = (int *)arg;
    int my_id = *ptr;
    printf("[Fix B] I am Thread %d (Passed by Heap Memory)\n", my_id);
    
    // IMPORTANT: Free the memory allocated in main
    free(arg); 
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int i;

    printf("=== 1. Demonstrating The Argument Trap (Wrong) ===\n");
    for(i = 0; i < NUM_THREADS; i++) {
        // ERROR: Passing address of 'i'. 'i' is shared and changing!
        pthread_create(&threads[i], NULL, print_id_wrong, (void *)&i);
    }
    for(i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);

    printf("\n=== 2. Fix A: Pass by Value (Cast to void*) ===\n");
    for(i = 0; i < NUM_THREADS; i++) {
        // CORRECT: Passing the VALUE of 'i'. 
        // We cast 'i' to 'long' first to avoid warnings on 64-bit systems.
        pthread_create(&threads[i], NULL, print_id_value, (void *)(long)i);
    }
    for(i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);

    printf("\n=== 3. Fix B: Pass by Malloc (Heap Allocation) ===\n");
    for(i = 0; i < NUM_THREADS; i++) {
        // CORRECT: Allocate new memory for each thread.
        // Each thread gets a unique pointer to its own integer.
        int *arg = malloc(sizeof(int));
        *arg = i;
        if (pthread_create(&threads[i], NULL, print_id_malloc, (void *)arg) != 0) {
            perror("Failed to create thread");
            free(arg); // Clean up if creation fails
        }
    }
    for(i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);

    printf("\nDone.\n");
    return 0;
}
