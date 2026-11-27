#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// Structure to pass multiple arguments to a thread
typedef struct {
    int thread_id;
    char *message;
} thread_data_t;

// Thread function
void *print_hello(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    printf("[Thread %d] Starting... Message: %s\n", data->thread_id, data->message);
    sleep(1); // Simulate work
    printf("[Thread %d] Done.\n", data->thread_id);
    
    // Free the memory allocated in main
    free(data);
    return NULL;
}

int main() {
    pthread_t threads[2];
    int i;

    printf("[Main] Creating threads...\n");

    for(i = 0; i < 2; i++) {
        // Correct Way: Allocate memory for arguments to avoid race conditions
        thread_data_t *t_data = malloc(sizeof(thread_data_t));
        t_data->thread_id = i + 1;
        t_data->message = (i == 0) ? "Hello" : "World";

        int res = pthread_create(&threads[i], NULL, print_hello, (void *)t_data);
        if (res) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(1);
        }
    }

    printf("[Main] Waiting for threads to finish...\n");

    // Join threads to ensure they finish before main exits
    for(i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[Main] All threads joined. Exiting.\n");
    return 0;
}
