#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>     // For time()
#include <sys/types.h>

#define SEM_NAME "/fork_demo_sem"
#define NUM_CHILDREN 5

void child_process(int id) {
    // 1. Open the existing named semaphore
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Child failed to open semaphore");
        exit(1);
    }

    // --- RANDOM DELAY START ---
    // Seed the random generator using time and PID to ensure uniqueness
    srand(time(NULL) ^ getpid()); 
    
    // Generate a random delay between 100ms and 1000ms
    int delay_ms = (rand() % 900) + 100; 

    printf("[Child %d] Born. Sleeping for %d ms before competing...\n", id, delay_ms);
    usleep(delay_ms * 1000); // usleep takes microseconds
    // --- RANDOM DELAY END ---

    printf("[Child %d] Awake and requesting lock (Wait)...\n", id);

    // 2. P Operation (Wait): Lock
    sem_wait(sem);

    // --- CRITICAL SECTION START ---
    printf("[Child %d] >>> INSIDE Critical Section <<<\n", id);
    sleep(1); // Simulate work inside CS
    printf("[Child %d] <<< LEAVING Critical Section <<<\n", id);
    // --- CRITICAL SECTION END ---

    // 3. V Operation (Post): Unlock
    sem_post(sem);
    sem_close(sem);
    exit(0);
}

int main() {
    // 1. Create the named semaphore in the parent
    // Initial value = 1 (Mutex)
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    
    // Cleanup if it already exists from a crashed run
    if (sem == SEM_FAILED) {
        sem_unlink(SEM_NAME);
        sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    }
    
    printf("[Parent] Semaphore created. Forking %d children...\n", NUM_CHILDREN);

    // 2. Fork children
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            child_process(i + 1);
        } else if (pid < 0) {
            perror("Fork failed");
        }
    }

    // 3. Wait for all children to finish
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    // 4. Cleanup
    printf("[Parent] All children finished. Cleaning up.\n");
    sem_close(sem);
    sem_unlink(SEM_NAME);

    return 0;
}
