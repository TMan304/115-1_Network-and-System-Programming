#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>

void *urgent_task(void *arg) {
    printf("[Urgent] Running with HIGH priority!\n");
    sleep(2);
    return NULL;
}

void *background_task(void *arg) {
    printf("[Background] Running detached. Main won't join me.\n");
    sleep(1);
    return NULL;
}

int main() {
    pthread_t t_urgent, t_bg;
    pthread_attr_t attr_urgent, attr_bg;
    struct sched_param param;
    int res;

    // --- 1. Setting up DETACHED attribute ---
    pthread_attr_init(&attr_bg);
    pthread_attr_setdetachstate(&attr_bg, PTHREAD_CREATE_DETACHED);
    pthread_create(&t_bg, &attr_bg, background_task, NULL);
    pthread_attr_destroy(&attr_bg);

    // --- 2. Setting up PRIORITY attribute ---
    pthread_attr_init(&attr_urgent);
    
    // We must use a realtime policy (FIFO or RR) to support static priorities
    res = pthread_attr_setschedpolicy(&attr_urgent, SCHED_FIFO);
    if (res != 0) perror("Failed to set policy (Needs Root?)");

    // Set priority to maximum allowed by the policy
    int max_prio = sched_get_priority_max(SCHED_FIFO);
    param.sched_priority = max_prio;
    
    res = pthread_attr_setschedparam(&attr_urgent, &param);
    
    // Explicitly tell pthreads to use our attributes, not inherit from parent
    pthread_attr_setinheritsched(&attr_urgent, PTHREAD_EXPLICIT_SCHED);

    res = pthread_create(&t_urgent, &attr_urgent, urgent_task, NULL);
    if (res != 0) {
        printf("[Main] Warning: Failed to set Realtime priority. Are you Root?\n");
        // Fallback: Create without special attributes
        pthread_create(&t_urgent, NULL, urgent_task, NULL);
    } else {
        printf("[Main] Created High Priority Thread (%d)\n", max_prio);
        pthread_join(t_urgent, NULL);
    }
    
    pthread_attr_destroy(&attr_urgent);
    
    printf("[Main] Exiting.\n");
    return 0;
}
