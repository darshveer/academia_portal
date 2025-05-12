#ifndef ADMIN_ACTIONS_H
#define ADMIN_ACTIONS_H

#include <sys/stat.h>
#include <sys/file.h>
#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 256
#define SUCCESS         0
#define FILE_ERROR     -1
#define USER_NOT_FOUND -2
#define DUPLICATE_ID   -3

/**
 * Prints formatted list of users from CSV file
 * @param filename Path to CSV file containing user data
 *
 * @brief THe function uses fcntl locking to ensure that only one process can read the file at a time.
 * The file is locked for the duration of the function call, and the lock is released when the
 * function returns.
 *
 * If the file does not contain the "active" field, the function will print a table without the
 * "Status" column.
 */
static inline void print_users(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    struct flock lock = {.l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Failed to acquire read lock");
        close(fd);
        return;
    }

    FILE *file = fdopen(fd, "r");
    if (!file) {
        perror("fdopen failed");
        close(fd);
        return;
    }

    char line[MAX_LINE];
    // Check if the file contains the "active" field
    int has_active_field = (fgets(line, sizeof(line), file) && strstr(line, "active")) ? 1 : 0;
    if (!has_active_field) rewind(file);

    if (has_active_field) {
        printf("╔════════════╦══════════════════════════════╦════════════════════════════════════╦════════════╗\n");
        printf("║   User ID  ║           Name               ║             Email                  ║   Status   ║\n");
        printf("╠════════════╬══════════════════════════════╬════════════════════════════════════╬════════════╣\n");
    } else {
        printf("╔════════════╦══════════════════════════════╦════════════════════════════════════╗\n");
        printf("║   User ID  ║           Name               ║             Email                  ║\n");
        printf("╠════════════╬══════════════════════════════╬════════════════════════════════════╣\n");
    }

    while (fgets(line, sizeof(line), file)) {
        int id, active = -1;
        char name[MAX_LINE] = {0}, email[MAX_LINE] = {0};

        if (has_active_field) {
            // Parse the line and get the user ID, name, email and status
            if (sscanf(line, "%d,%255[^,],%255[^,],%*[^,],%d", &id, name, email, &active) == 4) {
                name[strcspn(name, "\r\n")] = 0;
                email[strcspn(email, "\r\n")] = 0;
                printf("║ %-10d ║ %-28s ║ %-34s ║ %-10s ║\n", id, name, email, (active == 1) ? "Active" : "Inactive");
            }
        } else if (sscanf(line, "%d,%255[^,],%255[^,]", &id, name, email) >= 3) {
            name[strcspn(name, "\r\n")] = 0;
            email[strcspn(email, "\r\n")] = 0;
            printf("║ %-10d ║ %-28s ║ %-34s ║\n", id, name, email);
        }
    }

    printf(has_active_field ? "╚════════════╩══════════════════════════════╩════════════════════════════════════╩════════════╝\n" 
                           : "╚════════════╩══════════════════════════════╩════════════════════════════════════╝\n");

    // Release the lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(file);
}

/**
 * @brief Checks if a student ID exists in the database
 *
 * Opens the file in read-only mode, acquires a shared (read) lock, and checks each line of the file
 * to see if the student ID exists. If it does, the function releases the lock and returns 1. If it
 * doesn't, the function releases the lock and returns 0. If the file can't be opened, the function
 * returns FILE_ERROR.
 *
 * @param filename Path to student database file
 * @param id Student ID to check
 * @return 1 if exists, 0 if not, FILE_ERROR on failure
 */
static inline int check_student_id_exists(const char *filename, int id) {
    // Open the file in read-only mode
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return FILE_ERROR; // Return error if file can't be opened

    // Initialize file lock structure for a shared (read) lock
    struct flock lock = {.l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock"); // Print error if lock acquisition fails
        close(fd);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    // Read each line of the file
    while (read_line(fd, line, sizeof(line)) > 0) {
        int existing_id;
        // Parse the line for student ID and compare
        if (sscanf(line, "%d,", &existing_id) == 1 && existing_id == id) {
            // Release lock and close if ID is found
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);
            close(fd);
            return 1;
        }
    }

    // Release lock and close if ID is not found
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

/**
 * @brief Adds a new student record to the database
 * @param filename Path to student database file
 * @param id Student ID
 * @param name Student name
 * @param email Student email
 * @param password Student password
 * @param active Activation status (1 for active, 0 for inactive)
 * @return SUCCESS on success, DUPLICATE_ID if ID exists, FILE_ERROR on failure
 */
static inline int add_student(const char *filename, int id, const char *name, 
                             const char *email, const char *password, int active) {
    // Check if student ID already exists
    if (check_student_id_exists(filename, id)) return DUPLICATE_ID;

    // Open file in write-only mode, create if doesn't exist, append to end
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return FILE_ERROR; // Return error if file can't be opened

    // Acquire exclusive lock for writing
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock"); // Print error if lock acquisition fails
        close(fd);
        return FILE_ERROR;
    }

    // If file is empty, write header
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_size == 0) {
        dprintf(fd, "id,name,email,password,active,enrolled_courses\n");
    }

    // Write student data
    dprintf(fd, "%d,%s,%s,%s,%d,\n", id, name, email, password, active);

    // Release lock and close
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return SUCCESS;
}

/**
 * @brief Checks if a faculty ID exists in the database
 * @param filename Path to faculty database file
 * @param id Faculty ID to check
 * @return 1 if exists, 0 if not, FILE_ERROR on failure
 */
static inline int check_faculty_id_exists(const char *filename, int id) {
    int fd = open(filename, O_RDONLY); // Open file in read-only mode
    if (fd < 0) return FILE_ERROR; // Return error if file can't be opened

    // Acquire shared lock for reading
    struct flock lock = {.l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock"); // Print error if lock acquisition fails
        close(fd);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    while (read_line(fd, line, sizeof(line)) > 0) {
        int existing_id;
        // Parse the line for faculty ID and compare
        if (sscanf(line, "%d,", &existing_id) == 1 && existing_id == id) {
            // Release lock and close if ID is found
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);
            close(fd);
            return 1;
        }
    }

    // Release lock and close if ID is not found
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

/**
 * @brief Adds a new faculty record to the database
 * @param filename Path to faculty database file
 * @param id Faculty ID
 * @param name Faculty name
 * @param email Faculty email
 * @param password Faculty password
 * @return SUCCESS on success, DUPLICATE_ID if ID exists, FILE_ERROR on failure
 */
static inline int add_faculty(const char *filename, int id, const char *name, const char *email, const char *password) {
    // Check if ID already exists
    if (check_faculty_id_exists(filename, id)) return DUPLICATE_ID;

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return FILE_ERROR;

    // Acquire exclusive lock for writing
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock"); // Print error if lock acquisition fails
        close(fd);
        return FILE_ERROR;
    }

    // If file is empty, write header
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_size == 0) {
        dprintf(fd, "id,name,email,password,offered_courses\n");
    }

    // Write faculty data
    dprintf(fd, "%d,%s,%s,%s,\n", id, name, email, password);

    // Release lock and close
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return SUCCESS;
}

/**
 * @brief Updates user details in the database
 * @param filename Path to user database file
 * @param user_id ID of user to update
 * @param field_choice Field to update (1=name, 2=email, 3=password, 4=toggle active)
 * @param new_value New value for the field
 * @return SUCCESS on success, USER_NOT_FOUND if user not found, FILE_ERROR on failure
 */
static inline int update_user_details(const char *filename, int user_id, int field_choice, const char *new_value) {
    int fd = open(filename, O_RDWR);
    if (fd < 0) return FILE_ERROR;

    // Skip the header line if present
    char line[MAX_LINE];
    off_t offset = lseek(fd, 0, SEEK_SET);
    if (read(fd, line, 4) == 4 && strncmp(line, "id,", 3) == 0) {
        // Move to the end of the header line
        char c;
        while (read(fd, &c, 1) == 1 && c != '\n');
        offset = lseek(fd, 0, SEEK_CUR);
    }

    // Iterate over the lines in the file
    while (1) {
        ssize_t n = pread(fd, line, sizeof(line)-1, offset);
        if (n <= 0) break;
        line[n] = '\0';

        // Get the length of the line
        char *nl = strchr(line, '\n');
        int linelen = nl ? (nl - line + 1) : n;

        // Parse the line
        int id, active = -1;
        char name[MAX_LINE], email[MAX_LINE], pass[MAX_LINE];
        int fields = sscanf(line, "%d,%255[^,],%255[^,],%255[^,],%d", &id, name, email, pass, &active);

        // Check if the ID matches and if the line has the correct number of fields
        if ((fields == 4 || fields == 5) && id == user_id) {
            struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = offset, .l_len = linelen};
            if (fcntl(fd, F_SETLKW, &lock) < 0) {
                perror("Lock failed");
                close(fd);
                return FILE_ERROR;
            }

            // Update the correct field
            if (field_choice == 1) strncpy(name, new_value, sizeof(name) - 1);
            else if (field_choice == 2) strncpy(email, new_value, sizeof(email) - 1);
            else if (field_choice == 3) strncpy(pass, new_value, sizeof(pass) - 1);
            else if (field_choice == 4 && fields == 5) active = !active;

            // Construct the updated line
            char updated[MAX_LINE];
            int w = (fields == 5) ? snprintf(updated, sizeof(updated), "%d,%s,%s,%s,%d,\n", id, name, email, pass, active)
                                  : snprintf(updated, sizeof(updated), "%d,%s,%s,%s\n", id, name, email, pass);

            // Write the updated line
            pwrite(fd, updated, w, offset);

            // Release the lock
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &lock);

            // Close and return success
            close(fd);
            return SUCCESS;
        }

        // Move to the next line
        offset += linelen;
    }

    close(fd);
    return USER_NOT_FOUND;
}

/**
 * @brief Displays detailed information about a user
 * @param filename Path to user database file
 * @param user_id ID of user to view
 * @return SUCCESS on success, USER_NOT_FOUND if user not found, FILE_ERROR on failure
 */
static inline int view_user_details(const char *filename, int user_id) {
    int fd = open(filename, O_RDONLY); // Open the file in read-only mode
    if (fd < 0) {
        perror("open failed"); // Print error message if open fails
        return FILE_ERROR; // Return error code
    }

    struct flock lock = {.l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0}; // Create a read lock
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("read lock failed"); // Print error message if lock fails
        close(fd); // Close the file
        return FILE_ERROR; // Return error code
    }

    FILE *file = fdopen(fd, "r"); // Open the file as a FILE stream
    if (!file) {
        perror("fdopen failed"); // Print error message if fdopen fails
        close(fd); // Close the file
        return FILE_ERROR; // Return error code
    }

    char line[MAX_LINE]; // Initialize a character array to store a line
    int is_student = (fgets(line, sizeof(line), file) && strstr(line, "active") && strstr(line, "enrolled_courses") ? 1 : 0); // Check if the file contains the "active" and "enrolled_courses" fields
    rewind(file); // Go back to the beginning of the file

    while (fgets(line, sizeof(line), file)) { // Loop through each line of the file
        int id = 0, active = -1; // Initialize variables for the ID and status
        char name[MAX_LINE] = {0}, email[MAX_LINE] = {0}, password[MAX_LINE] = {0}, extra[MAX_LINE] = {0}; // Initialize character arrays for the name, email, password, and extra fields

        if (is_student) { // If the user is a student
            if (sscanf(line, "%d,%255[^,],%255[^,],%255[^,],%d,%255[^\n]", &id, name, email, password, &active, extra) >= 5 && id == user_id) { // Parse the line and check if the ID matches
                printf("\n╔══════════════════════════════════════════════════════╗\n"); // Print the header
                printf("║               STUDENT DETAILS                         ║\n"); // Print the student details label
                printf("╠══════════════════════════════════════════════════════╣\n"); // Print the separator
                printf("║ ID: %-49d ║\n", id); // Print the ID
                printf("║ Name: %-47s ║\n", name); // Print the name
                printf("║ Email: %-46s ║\n", email); // Print the email
                printf("║ Password: %-43s ║\n", password); // Print the password
                printf("║ Status: %-45s ║\n", (active == 1) ? "Active" : "Inactive"); // Print the status
                printf("║ Enrolled Courses: %-34s ║\n", (extra[0] ? extra : "(none)")); // Print the enrolled courses
                printf("╚══════════════════════════════════════════════════════╝\n"); // Print the footer
                fclose(file); // Close the file
                return SUCCESS; // Return success
            }
        } else if (sscanf(line, "%d,%255[^,],%255[^,],%255[^,],%255[^\n]", &id, name, email, password, extra) >= 4 && id == user_id) { // If the user is a faculty
            printf("\n╔══════════════════════════════════════════════════════╗\n"); // Print the header
            printf("║               FACULTY DETAILS                        ║\n"); // Print the faculty details label
            printf("╠══════════════════════════════════════════════════════╣\n"); // Print the separator
            printf("║ ID: %-49d║\n", id); // Print the ID
            printf("║ Name: %-47s║\n", name); // Print the name
            printf("║ Email: %-46s║\n", email); // Print the email
            printf("║ Password: %-43s║\n", password); // Print the password
            if (extra[0]) printf("║ Offered Courses: %-36s║\n", extra); // Print the offered courses
            printf("╚══════════════════════════════════════════════════════╝\n"); // Print the footer
            fclose(file); // Close the file
            return SUCCESS; // Return success
        }
    }

    fclose(file); // Close the file
    return USER_NOT_FOUND; // Return error code if the user is not found
}

#endif // ADMIN_ACTIONS_H