#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 256

// TODO: Change this to a hash map with separate chaining
// for efficient reads/writes (O(N) -> O(1))
typedef struct {
    char key[256];
    char value[256];
} kv_pair;

kv_pair kv_store[256];
int kv_count = 0;

void set_nonblocking(int sockfd) {
    int opts = fcntl(sockfd, F_GETFL);
    opts = (opts | O_NONBLOCK);
    fcntl(sockfd, F_SETFL, opts);
}

int create_server_socket() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(socket_fd, 10);
    set_nonblocking(socket_fd);
    return socket_fd;
}

void handle_client_request(int client_fd) {
    // Read data from client_fd
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("Client disconnected\n");
        } else {
            perror("read");
        }
        close(client_fd);
        return;
    }

    // Place null char at end of the buffer
    buffer[bytes_read] = '\0';  

    // If it starts with GET, execute GET command
    if (strncmp(buffer, "GET", strlen("GET")) == 0) {
        char key[256];
        sscanf(buffer, "GET %s", key);

        for (int i = 0; i < kv_count; ++i) {
            if (strcmp(kv_store[i].key, key) == 0) {
                write(client_fd, strcat(kv_store[i].value, "\n"), strlen(kv_store[i].value) + 1);
                return;
            }
        }
        write(client_fd, "Key not found\n", strlen("Key not found\n"));
    }

    // If it starts with SET, execute SET command
    if (strncmp(buffer, "SET", strlen("SET")) == 0) {
        char val[256];
        char key[256];
        sscanf(buffer + strlen("SET "), "%s %s", key, val);
        
        for (int i = 0; i < kv_count; ++i) {
            if (strcmp(kv_store[i].key, key) == 0) {
                strcpy(kv_store[i].value, val);
                write(client_fd, "Successfully set\n", strlen("Successfully set\n"));
                return;
            }
        }

        // If key is not found, add new key-value pair
        strcpy(kv_store[kv_count].key, key);
        strcpy(kv_store[kv_count].value, val);
        kv_count++;
        write(client_fd, "Successfully set\n", strlen("Successfully set\n"));
    }
}

int main() {
    int server_fd = create_server_socket();

    // Create epoll instance to monitor multiple file descriptors
    // in O(1) time
    int epoll_fd = epoll_create1(0);  
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Create struct to store epoll events
    struct epoll_event ev, events[256];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }

    // Main loop to handle client requests
    while (1) {
        // Wait for an event to occur on one of the file descriptors
        int num_clients = epoll_wait(epoll_fd, events, 256, -1);
        if (num_clients == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < num_clients; ++i) {
            // Check for new client connections
            // Any new connection requests to the server socket will generate an event.
            if (events[i].data.fd == server_fd) {
                // Accept new client connection
                int client_fd = accept(server_fd, NULL, NULL); 
                set_nonblocking(client_fd);
                ev.events = EPOLLIN | EPOLLET; // Edge-triggered mode
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                handle_client_request(events[i].data.fd);
            }
        }
    }
    return 0;
}
