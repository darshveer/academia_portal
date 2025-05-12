#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <string.h>

// Add centralized error definitions at the top of the file
#define SUCCESS         0
#define FAILURE        -1
#define FILE_ERROR     -1
#define USER_NOT_FOUND -2
#define NOT_FOUND     -2
#define COURSE_NOT_FOUND -3
#define DUPLICATE_ID   -3
#define ALREADY_ENROLLED -4
#define NOT_ENROLLED   -5
#define DEACTIVATED    -3
#define INCORRECT_ROLE -4
#define WRONG_PASS     -1
#define WRONG_USER     -2

/**
 * @brief Reads a line from a file descriptor into a buffer.
 * 
 * This function reads characters one by one from the file descriptor until
 * it encounters a newline character or reaches the maximum buffer length.
 * It is safe to use with `fcntl()` locking since it uses low-level `read()`.
 * 
 * @param fd       File descriptor to read from.
 * @param buffer   Pointer to buffer to store the line.
 * @param max_len  Maximum number of bytes to read (including null terminator).
 * @return ssize_t Number of bytes read, 0 on EOF, -1 on error.
 */
static inline ssize_t read_line(int fd, char *buffer, size_t max_len) {
    size_t i = 0;
    char c;
    ssize_t bytes_read;

    // Read characters one by one until newline or max length
    while (i < max_len - 1) {
        bytes_read = read(fd, &c, 1);
        
        // Check for EOF or error
        if (bytes_read <= 0) {
            if (i == 0) return bytes_read; // EOF or error before reading anything
            break;
        }

        // Store character and check for newline
        buffer[i++] = c;
        if (c == '\n') break;
    }

    // Null-terminate the string
    buffer[i] = '\0';
    return i;
}

#endif // UTILS_H
