#ifndef SERVER_H
#define SERVER_H

/**
 * @brief Authenticates a connected user based on role.
 * 
 * @param client_fd File descriptor for the connected client socket.
 * @param role User role (1 = Admin, 2 = Student, 3 = Faculty).
 * @return int 
 *         > 0 on success (user ID), 
 *         <= 0 on failure.
 */
int authenticate_user(int client_fd, int role);

/**
 * @brief Handles the communication lifecycle with a connected client.
 * 
 * @param client_fd Socket file descriptor for the client.
 * @return int 0 on success, -1 on failure or disconnection.
 */
int handle_client(int client_fd);

/**
 * @brief Creates, binds, and listens on a TCP socket on the given port.
 * 
 * @param port Port number to listen on.
 * @return int Socket file descriptor of the server.
 */
int setup_server_socket(int port);

/**
 * @brief Entry point for handling admin-specific client operations.
 * 
 * @param client_fd Socket descriptor for the admin client.
 */
void handle_admin_actions(int client_fd);

/**
 * @brief Entry point for handling student-specific client operations.
 * 
 * @param client_fd Socket descriptor for the student client.
 */
void handle_student_actions(int client_fd);

/**
 * @brief Entry point for handling faculty-specific client operations.
 * 
 * @param client_fd Socket descriptor for the faculty client.
 */
void handle_faculty_actions(int client_fd);

#endif // SERVER_H
