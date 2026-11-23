#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <sys/stat.h>

#define SHM_NAME "/demo_shm"
#define SEM_NAME "/demo_sem"
#define SHM_SIZE sizeof(int) // We only store one integer
#define NUM_CHILDREN 2

void child_process(int id, int *shared_counter, sem_t *sem) {
    // --- RANDOM DELAY START ---
    // Seed random generator with PID to differentiate children
    srand(time(NULL) ^ getpid());
    int delay_ms = (rand() % 900) + 100; // 100ms to 1000ms

    printf("[Child %d] Born. Sleeping for %d ms...\n", id, delay_ms);
    usleep(delay_ms * 1000);
    // --- RANDOM DELAY END ---

    printf("[Child %d] Waking up. Trying to lock Semaphore...\n", id);

    // 1. Lock (Critical Section Start)
    sem_wait(sem);

    // --- CRITICAL SECTION (Accessing Shared Memory) ---
    int temp = *shared_counter;
    printf("[Child %d] Read Counter: %d. Incrementing...\n", id, temp);
    sleep(1); // Artificial delay to prove the lock works
    *shared_counter = temp + 1;
    printf("[Child %d] Wrote Counter: %d.\n", id, *shared_counter);
    // --- CRITICAL SECTION END ---

    // 2. Unlock
    sem_post(sem);
    exit(0);
}

int main() {
    int shm_fd;
    int *shared_counter;
    sem_t *sem;

    // 1. Create Shared Memory Object
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    // 2. Configure size of Shared Memory
    ftruncate(shm_fd, SHM_SIZE);

    // 3. Map Shared Memory into this process's address space
    // PROT_READ | PROT_WRITE: Readable and Writable
    // MAP_SHARED: Changes are visible to other processes
    shared_counter = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_counter == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    // Initialize counter to 0
    *shared_counter = 0;

    // 4. Create Semaphore for synchronization
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) {
        // Cleanup if previously crashed
        sem_unlink(SEM_NAME);
        sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    }

    printf("[Parent] Setup complete. Counter = 0. Forking children...\n");

    // 5. Fork Children
    // Note: Children inherit the mmap pointer and semaphore pointer
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            child_process(i + 1, shared_counter, sem);
        }
    }

    // 6. Wait for children
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    printf("[Parent] All children finished. Final Counter: %d\n", *shared_counter);

    // 7. Cleanup
    munmap(shared_counter, SHM_SIZE); // Unmap memory
    shm_unlink(SHM_NAME);             // Remove SHM object from system
    sem_close(sem);                   // Close semaphore
    sem_unlink(SEM_NAME);             // Remove Semaphore from system

    return 0;
}
