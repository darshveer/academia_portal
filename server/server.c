#include "server.h"
#include "auth.h"
#include "types.h"
#include "../client/admin_client.h"
#include "../client/faculty_client.h"
#include "../client/student_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>

/**
 * @brief Entry point for the server.
 *        Sets up socket, listens for connections, forks for each client.
 * 
 * @return int Exit status.
 */
int main() {
    system("clear");
    int server_fd = setup_server_socket(PORT);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        printf("Waiting for a new connection...\n");

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Accept new client connection
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Client Connection");
            continue;
        }

        // Fork a child to handle each client separately
        if (fork() == 0) {
            close(server_fd);          // child doesn't need the listener socket
            handle_client(client_fd);  // handle authentication and interaction
            close(client_fd);
            exit(0);                   // child exits after handling client
        }

        // Parent process: close client_fd as child is handling it
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

/**
 * @brief Sets up a TCP socket bound to the given port.
 * 
 * @param port Port number to bind the server.
 * @return int Server socket file descriptor.
 */
int setup_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket Creation");
        exit(EXIT_FAILURE);
    }

    // Allow reuse of address (avoids "address already in use" on restart)
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set server address and port
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on all interfaces
    server_addr.sin_port = htons(port);

    // Bind the socket to the address and port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(sockfd, 5) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

/**
 * @brief Handles an individual client connection.
 *        Authenticates user and maintains connection until client disconnects.
 * 
 * @param client_fd Socket file descriptor for connected client.
 * @return int Status (0 on success, -1 on auth failure).
 */
int handle_client(int client_fd) {
    int role;

    // Prompt user to select a role (Admin, Student, Faculty)
    write(client_fd, "Enter role (1-Admin, 2-Student, 3-Faculty): ", 45);
    if (read(client_fd, &role, sizeof(role)) <= 0) {
        perror("Failed to read role");
        return -1;
    }

    // Validate role
    if (role < 1 || role > 3) {
        int status = INCORRECT_ROLE;
        write(client_fd, &status, sizeof(int));
        close(client_fd);     // Close client socket
        exit(EXIT_FAILURE);   // Terminate handler
    }

    // Authenticate the client
    int auth_result = authenticate_user(client_fd, role);
    if (auth_result <= 0) {
        close(client_fd);
        return -1;
    }

    // If authentication succeeded, keep connection alive until client disconnects
    char buffer[1024];
    while (read(client_fd, buffer, sizeof(buffer)) > 0) {
        // No server interaction after auth; keeps connection alive
    }

    return 0;
}
