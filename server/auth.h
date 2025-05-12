#include "server.h"
#include "types.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LOGIN_SUCCESS 1
#define WRONG_PASS -1
#define WRONG_USER -2
#define DEACTIVATED -3
#define INCORRECT_ROLE -4

/**
 * @brief Authenticates a user (student, faculty, or admin) by reading from the corresponding CSV file.
 * 
 * @param client_fd File descriptor for the connected client.
 * @param role User role: STUDENT, FACULTY, or ADMIN.
 * 
 * @return int 
 *         LOGIN_SUCCESS (1)     on successful login,
 *         WRONG_PASS (-1)       if password mismatch,
 *         WRONG_USER (-2)       if email not found,
 *         DEACTIVATED (-3)      if student account is inactive,
 *         INCORRECT_ROLE (-4)   if role is unrecognized.
 */
int authenticate_user(int client_fd, int role) {
    char email[MAX_EMAIL_LEN] = {0};
    char password[MAX_PASS_LEN] = {0};

    // Prompt client for email and read input
    write(client_fd, "Enter email: ", 14);
    ssize_t bytes_read = read(client_fd, email, MAX_EMAIL_LEN - 1);
    if (bytes_read <= 0) return WRONG_USER; // Handle read error
    email[bytes_read] = '\0';
    email[strcspn(email, "\r\n")] = '\0'; // Remove newline characters

    // Prompt client for password and read input
    write(client_fd, "Enter password: ", 17);
    bytes_read = read(client_fd, password, MAX_PASS_LEN - 1);
    if (bytes_read <= 0) return WRONG_USER; // Handle read error
    password[bytes_read] = '\0';
    password[strcspn(password, "\r\n")] = '\0'; // Remove newline characters

    // Determine the correct database file based on role
    const char *filename = NULL;
    switch (role) {
        case STUDENT:  filename = "../database/students.csv"; break;
        case ADMIN:    filename = "../database/admins.csv";   break;
        case FACULTY:  filename = "../database/faculty.csv";  break;
        default:       return INCORRECT_ROLE; // Invalid role
    }

    // Open the user database file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open user database");
        int status = WRONG_USER;
        write(client_fd, &status, sizeof(int));
        return WRONG_USER;
    }

    // Set up file lock for reading
    struct flock lock = {
        .l_type = F_RDLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0
    };

    // Acquire file lock
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Failed to acquire read lock");
        close(fd);
        int status = WRONG_USER;
        write(client_fd, &status, sizeof(int));
        return WRONG_USER;
    }

    // Convert file descriptor to FILE pointer for easier reading
    FILE *file = fdopen(fd, "r");
    if (!file) {
        perror("fdopen failed");
        close(fd);
        int status = WRONG_USER;
        write(client_fd, &status, sizeof(int));
        return WRONG_USER;
    }

    char line[256];
    // Check for header and skip if present
    if (fgets(line, sizeof(line), file) && strncmp(line, "id,", 3) == 0) {
        // Skip header
    } else {
        rewind(file);
    }

    // Read and process each line from the file
    while (fgets(line, sizeof(line), file)) {
        int id = 0;
        char name[MAX_NAME_LEN] = {0};
        char mail[MAX_EMAIL_LEN] = {0};
        char pass[MAX_PASS_LEN] = {0};

        // Parse the line based on role
        if (role == STUDENT) {
            int active = 0;
            if (sscanf(line, "%d,%[^,],%[^,],%[^,],%d", &id, name, mail, pass, &active) < 5)
                continue; // Skip malformed lines

            mail[strcspn(mail, "\r\n")] = '\0';
            pass[strcspn(pass, "\r\n")] = '\0';

            if (strcmp(mail, email) == 0) {
                if (strcmp(pass, password) == 0) {
                    if (!active) {
                        int status = DEACTIVATED;
                        write(client_fd, &status, sizeof(int));
                        goto cleanup; // Account is inactive
                    }

                    int status = LOGIN_SUCCESS;
                    write(client_fd, &status, sizeof(int));
                    write(client_fd, &id, sizeof(int));

                    char welcome_msg[256];
                    snprintf(welcome_msg, sizeof(welcome_msg),
                             " Welcome %s! You are logged in as Student.                ║\n",
                             name);
                    write(client_fd, welcome_msg, strlen(welcome_msg));
                    goto cleanup;
                } else {
                    int status = WRONG_PASS;
                    write(client_fd, &status, sizeof(int));
                    goto cleanup; // Password mismatch
                }
            }
        } else {
            if (sscanf(line, "%d,%[^,],%[^,],%[^,\n]", &id, name, mail, pass) < 4)
                continue; // Skip malformed lines

            mail[strcspn(mail, "\r\n")] = '\0';
            pass[strcspn(pass, "\r\n")] = '\0';

            if (strcmp(mail, email) == 0) {
                if (strcmp(pass, password) == 0) {
                    int status = LOGIN_SUCCESS;
                    write(client_fd, &status, sizeof(int));
                    write(client_fd, &id, sizeof(int));

                    const char *role_str = (role == ADMIN) ? "Administrator" : "Faculty";
                    char welcome_msg[256];
                    snprintf(welcome_msg, sizeof(welcome_msg),
                             " Welcome %s! You are logged in as %s.                ║\n",
                             name, role_str);
                    write(client_fd, welcome_msg, strlen(welcome_msg));
                    goto cleanup;
                } else {
                    int status = WRONG_PASS;
                    write(client_fd, &status, sizeof(int));
                    goto cleanup; // Password mismatch
                }
            }
        }
    }

    // If no matching user is found, send WRONG_USER status
    {
        int status = WRONG_USER;
        write(client_fd, &status, sizeof(int));
    }

cleanup:
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock); // Release the file lock
    fclose(file); // Close the file
    return 0;
}
