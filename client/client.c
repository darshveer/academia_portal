#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include "admin_client.h"
#include "student_client.h"
#include "faculty_client.h"
#include "../server/utils.h" // Add this include

#define LOGIN_SUCCESS 1
#define WRONG_PASS -1
#define WRONG_USER -2
#define DEACTIVATED -3
#define INCORRECT_ROLE -4

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
// Use the same MAX_BUF value as in student_client.h to avoid redefinition warning
#define CLIENT_BUF_SIZE 1024

// Remove the read_line function definition from here

// Update the main function to use system calls
int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[CLIENT_BUF_SIZE];
    int role;
    
    // Clear the screen
    system("clear");

    // Print welcome banner
    const char *banner = "\n╔═══════════════════════════════════════════════════════════════════╗"
                         "\n║                                                                   ║"
                         "\n║             ACADEMIA COURSE REGISTRATION PORTAL                   ║"
                         "\n║                                                                   ║"
                         "\n╚═══════════════════════════════════════════════════════════════════╝\n";
    write(STDOUT_FILENO, banner, strlen(banner));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    const char *connecting_msg = "\nConnecting to server...\n";
    write(STDOUT_FILENO, connecting_msg, strlen(connecting_msg));
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        const char *conn_failed = "\n╔═════════════════════════════════════════════════════════╗"
                                 "\n║ Connection failed! Please check if server is running.   ║"
                                 "\n╚═════════════════════════════════════════════════════════╝\n";
        write(STDOUT_FILENO, conn_failed, strlen(conn_failed));
        return -1;
    }
    
    const char *conn_success = "Connected successfully!\n";
    write(STDOUT_FILENO, conn_success, strlen(conn_success));

    // Read role prompt
    ssize_t bytes_read = read(sockfd, buffer, CLIENT_BUF_SIZE);
    if (bytes_read <= 0) {
        const char *read_failed = "Failed to read from server\n";
        write(STDERR_FILENO, read_failed, strlen(read_failed));
        close(sockfd);
        return -1;
    }
    buffer[bytes_read] = '\0';
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, buffer, bytes_read);

    // User selects role
    char role_buf[16];
    bytes_read = read(STDIN_FILENO, role_buf, sizeof(role_buf) - 1);
    if (bytes_read <= 0) {
        const char *invalid_input = "\n╔═════════════════════════╗"
                                   "\n║ Invalid input!          ║"
                                   "\n╚═════════════════════════╝\n";
        write(STDOUT_FILENO, invalid_input, strlen(invalid_input));
        close(sockfd);
        return -1;
    }
    role_buf[bytes_read] = '\0';
    role = atoi(role_buf);
    write(sockfd, &role, sizeof(role));

    if (role < 1 || role > 3) {
        const char *invalid_role = "\n╔═════════════════════════╗"
                                  "\n║ Invalid role selected!  ║"
                                  "\n╚═════════════════════════╝\n";
        write(STDOUT_FILENO, invalid_role, strlen(invalid_role));
        close(sockfd);
        exit(INCORRECT_ROLE);
    }

    // Handle authentication (read email, send password)
    read(sockfd, buffer, CLIENT_BUF_SIZE);
    write(STDOUT_FILENO, buffer, strlen(buffer));
    
    char email[CLIENT_BUF_SIZE];
    bytes_read = read(STDIN_FILENO, email, sizeof(email) - 1);
    if (bytes_read <= 0) {
        close(sockfd);
        return -1;
    }
    email[bytes_read] = '\0';
    write(sockfd, email, strlen(email));

    // Receive "Enter password: " from server
    read(sockfd, buffer, CLIENT_BUF_SIZE);
    write(STDOUT_FILENO, buffer, strlen(buffer));

    // Hide password input
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Read password from user
    char password[CLIENT_BUF_SIZE];
    bytes_read = read(STDIN_FILENO, password, sizeof(password) - 1);
    if (bytes_read <= 0) {
        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        close(sockfd);
        return -1;
    }
    password[bytes_read] = '\0';
    password[strcspn(password, "\n")] = '\0'; // Remove newline

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    write(STDOUT_FILENO, "\n", 1); // To move to next line after password input

    // Send password to server
    write(sockfd, password, strlen(password));

    int status, faculty_id = -1, student_id = -1, admin_id = -1;
    read(sockfd, &status, sizeof(status));

    if (status == LOGIN_SUCCESS) {
        // Read welcome message from server
        char welcome_msg[CLIENT_BUF_SIZE];
        memset(welcome_msg, 0, sizeof(welcome_msg));
        
        if (role == FACULTY) {
            read(sockfd, &faculty_id, sizeof(faculty_id));
        } else if (role == STUDENT) {
            read(sockfd, &student_id, sizeof(student_id));
        } else if (role == ADMIN) {
            read(sockfd, &admin_id, sizeof(admin_id));
        }
        
        // Read welcome message
        bytes_read = read(sockfd, welcome_msg, sizeof(welcome_msg) - 1);
        if (bytes_read > 0) {
            welcome_msg[bytes_read] = '\0';
            const char *welcome_header = "\n╔═════════════════════════════════════════════════════════╗\n║ ";
            write(STDOUT_FILENO, welcome_header, strlen(welcome_header));
            write(STDOUT_FILENO, welcome_msg, strlen(welcome_msg));
            const char *welcome_footer = "╚═════════════════════════════════════════════════════════╝\n";
            write(STDOUT_FILENO, welcome_footer, strlen(welcome_footer));
        }
    } else if (status == INCORRECT_ROLE) {
        const char *invalid_role = "\n╔═════════════════════════╗"
                                  "\n║ Invalid role selected!  ║"
                                  "\n╚═════════════════════════╝\n";
        write(STDOUT_FILENO, invalid_role, strlen(invalid_role));
        close(sockfd);
        exit(EXIT_FAILURE);
    } else {
        switch (status) {
            case WRONG_PASS: {
                const char *wrong_pass = "\n╔═════════════════════════╗"
                                        "\n║ Incorrect password!     ║"
                                        "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, wrong_pass, strlen(wrong_pass));
                break;
            }
            case WRONG_USER: {
                const char *wrong_user = "\n╔═════════════════════════╗"
                                        "\n║ User not found!         ║"
                                        "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, wrong_user, strlen(wrong_user));
                break;
            }
            case DEACTIVATED: {
                const char *deactivated = "\n╔═════════════════════════╗"
                                         "\n║ Account is deactivated! ║"
                                         "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, deactivated, strlen(deactivated));
                break;
            }
            default: {
                const char *auth_failed = "\n╔═════════════════════════╗"
                                         "\n║ Authentication failed!  ║"
                                         "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, auth_failed, strlen(auth_failed));
                break;
            }
        }
        close(sockfd);
        return 0;
    }

    // If authentication succeeded, dispatch to role-specific menu
    switch (role) {
        case 1: 
            handle_admin_menu(); 
            break;
        case 2: 
            handle_student_menu(student_id); 
            break;
        case 3: 
            handle_faculty_menu(faculty_id); 
            break;
        default: {
            const char *invalid_role = "\n╔═════════════════════════╗"
                                      "\n║ Invalid role!           ║"
                                      "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, invalid_role, strlen(invalid_role));
            break;
        }
    }

    close(sockfd);
    return 0;
}
