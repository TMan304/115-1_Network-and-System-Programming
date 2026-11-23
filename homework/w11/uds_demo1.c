#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>

#define SOCKET_PATH "/tmp/demo_socket"
#define BUFFER_SIZE 128

// --- CLIENT PROCESS (Child) ---
void client_process() {
    // Random delay to simulate real-world timing
    srand(time(NULL) ^ getpid());
    int delay_ms = (rand() % 500) + 100;
    usleep(delay_ms * 1000);

    printf("[Client] Starting up. Connecting to server...\n");

    // 1. Create Socket
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("[Client] socket error");
        exit(1);
    }

    // 2. Define Server Address
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // 3. Connect to Server
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("[Client] connect error");
        exit(1);
    }

    // 4. Send Message
    char *msg = "Hello Server, this is the Client!";
    printf("[Client] Sending: '%s'\n", msg);
    write(client_fd, msg, strlen(msg));

    // 5. Read Response
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("[Client] Received response: '%s'\n", buffer);
    }

    close(client_fd);
    exit(0);
}

// --- SERVER PROCESS (Parent) ---
int main() {
    // 1. Create Socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[Server] socket error");
        exit(1);
    }

    // 2. Bind to File Path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH); // Remove previous file if it exists
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("[Server] bind error");
        exit(1);
    }

    // 3. Listen for connections (Backlog = 5)
    if (listen(server_fd, 5) == -1) {
        perror("[Server] listen error");
        exit(1);
    }

    printf("[Server] Listening on %s...\n", SOCKET_PATH);

    // Fork the client *after* server is ready to listen
    pid_t pid = fork();
    if (pid == 0) {
        client_process();
    }

    // 4. Accept Connection
    int client_conn;
    if ((client_conn = accept(server_fd, NULL, NULL)) == -1) {
        perror("[Server] accept error");
        exit(1);
    }

    printf("[Server] Client connected!\n");

    // 5. Read Data
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_conn, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("[Server] Received: '%s'\n", buffer);
    }

    // 6. Send Response
    char *response = "Message received. Over and out.";
    write(client_conn, response, strlen(response));

    // 7. Cleanup
    wait(NULL); // Wait for child
    close(client_conn);
    close(server_fd);
    unlink(SOCKET_PATH); // Delete the socket file
    printf("[Server] Closed and cleaned up.\n");

    return 0;
}
