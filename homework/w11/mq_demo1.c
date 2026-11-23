#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <time.h>
#include <sys/wait.h>

#define MQ_NAME "/demo_mq"
#define MSG_SIZE 128
#define MAX_MSGS 10

// Define a simple message structure (optional, but good practice)
typedef struct {
    int id;
    char text[64];
} Message;

// --- PRODUCER CHILD (SENDER) ---
void producer_child(int id) {
    // Random delay setup
    srand(time(NULL) ^ getpid());
    int delay_ms = (rand() % 500) + 100; // Sleep 100-600ms

    printf("[Producer %d] Born. Sleeping %d ms...\n", id, delay_ms);
    usleep(delay_ms * 1000);

    // 1. Open existing MQ (Write Only)
    mqd_t mq = mq_open(MQ_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("Producer: mq_open failed");
        exit(1);
    }

    // 2. Prepare and Send Message
    Message msg;
    msg.id = id;
    snprintf(msg.text, sizeof(msg.text), "Hello from Child %d", id);

    printf("[Producer %d] Sending message: '%s'\n", id, msg.text);

    // mq_send(queue, data, size, priority)
    // Priority 0 is default. Higher numbers = higher priority.
    if (mq_send(mq, (const char*)&msg, sizeof(msg), 0) == -1) {
        perror("Producer: mq_send failed");
        exit(1);
    }

    printf("[Producer %d] Message sent. Exiting.\n", id);
    mq_close(mq);
    exit(0);
}

// --- CONSUMER CHILD (RECEIVER) ---
void consumer_child(int id) {
    printf("[Consumer %d] Born. Waiting for messages...\n", id);

    // 1. Open existing MQ (Read Only)
    mqd_t mq = mq_open(MQ_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) {
        perror("Consumer: mq_open failed");
        exit(1);
    }

    // 2. Receive Message
    // Note: buffer size must be >= msg_len defined in attr (MSG_SIZE)
    char buffer[MSG_SIZE]; 
    unsigned int priority;

    // mq_receive blocks until a message is available!
    ssize_t bytes_read = mq_receive(mq, buffer, MSG_SIZE, &priority);
    
    if (bytes_read == -1) {
        perror("Consumer: mq_receive failed");
        exit(1);
    }

    // Cast buffer back to struct to read data
    Message *msg = (Message*)buffer;
    printf("[Consumer %d] RECEIVED from ID %d: '%s' (Priority: %u)\n", 
           id, msg->id, msg->text, priority);

    mq_close(mq);
    exit(0);
}

int main() {
    // 1. Define Attributes for the Queue
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MSGS;  // Max messages in queue
    attr.mq_msgsize = MSG_SIZE; // Max size per message
    attr.mq_curmsgs = 0;

    // 2. Create the Message Queue
    // O_CREAT: Create if not exists
    // O_RDWR: Parent needs read/write to create, though children use partial permissions
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("Parent: mq_open failed");
        exit(1);
    }

    printf("[Parent] MQ created. Forking processes...\n");

    // 3. Fork Consumer first (so it's ready to wait)
    pid_t pid_c = fork();
    if (pid_c == 0) consumer_child(2); // ID 2 is Consumer

    // 4. Fork Producer
    pid_t pid_p = fork();
    if (pid_p == 0) producer_child(1); // ID 1 is Producer

    // 5. Wait for children
    wait(NULL);
    wait(NULL);

    // 6. Cleanup
    printf("[Parent] All children finished. Cleaning up.\n");
    mq_close(mq);
    mq_unlink(MQ_NAME); // Destroys the queue

    return 0;
}
