#ifndef STUDENT_ACTIONS_H
#define STUDENT_ACTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include "types.h"
#include "utils.h"

#define MAX_ERROR_MSG 512
#define MAX_BUFFER 2048
#define MAX_STUDENT_BUFFER 2048
#define MAX_COURSE_BUFFER 2048

#define DB_STUDENTS "../database/students.csv"
#define DB_COURSES "../database/courses.csv"
#define DB_FACULTY "../database/faculty.csv"

/**
 * @brief Lists available courses for a student to enroll in.
 *
 * This function checks the student's enrollment status and lists courses that are 
 * available for enrollment. It uses file locks to ensure safe concurrent access 
 * to the student, course, and faculty databases.
 *
 * @param student_id ID of the student.
 * @return SUCCESS on success, USER_NOT_FOUND if student is not found, FILE_ERROR on file operation errors.
 */
int list_available_courses(int student_id) {
    int student_fd = open(DB_STUDENTS, O_RDONLY);
    if (student_fd < 0) {
        const char *err_msg = "Error opening students file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        return FILE_ERROR;
    }
    
    // Acquire shared lock for reading student file
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(student_fd, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on students file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(student_fd);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    Student student;
    int found = 0;
    ssize_t bytes_read;

    // Find the student
    while ((bytes_read = read_line(student_fd, line, sizeof(line))) > 0) {
        char *token = strtok(line, ",");
        if (!token) continue;
        student.id = atoi(token);
        if (student.id == student_id) {
            token = strtok(NULL, ",");
            if (token) strcpy(student.name, token);
            token = strtok(NULL, ",");
            if (token) strcpy(student.email, token);
            token = strtok(NULL, ",");
            if (token) strcpy(student.password, token);
            token = strtok(NULL, ",");
            if (token) student.active = atoi(token);
            token = strtok(NULL, "\n");
            if (token) {
                strcpy(student.enrolled_courses, token);
            } else {
                student.enrolled_courses[0] = '\0';
            }
            found = 1;
            break;
        }
    }
    
    // Release lock on student file
    lock.l_type = F_UNLCK;
    fcntl(student_fd, F_SETLK, &lock);
    close(student_fd);

    if (!found) {
        // Use a larger buffer to avoid truncation
        char error_msg[MAX_ERROR_MSG];
        snprintf(error_msg, sizeof(error_msg),
                "\n╔════════════════════════════════════════════════╗"
                "\n║ Error: Student with ID %d not found in system. ║"
                "\n╚════════════════════════════════════════════════╝\n", 
                student_id);
        write(STDOUT_FILENO, error_msg, strlen(error_msg));
        return USER_NOT_FOUND;
    }

    int course_fd = open(DB_COURSES, O_RDONLY);
    if (course_fd < 0) {
        const char *err_msg = "Error opening courses file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        return FILE_ERROR;
    }
    
    // Acquire shared lock for reading course file
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(course_fd, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on courses file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(course_fd);
        return FILE_ERROR;
    }

    const char *header = "\n╔══════════════════════════════════════════════════════════════════════════════════════════╗"
                         "\n║                              AVAILABLE COURSES FOR ENROLLMENT                            ║"
                         "\n╠═══════════╦═══════════╦═══════════╦════════════════════════════════╦═════════════════════╣"
                         "\n║ Course ID ║   Code    ║  Credits  ║          Course Name           ║       Faculty       ║"
                         "\n╠═══════════╬═══════════╬═══════════╬════════════════════════════════╬═════════════════════╣";
    write(STDOUT_FILENO, header, strlen(header));

    int count = 0;

    // Skip header
    read_line(course_fd, line, sizeof(line));
    
    while ((bytes_read = read_line(course_fd, line, sizeof(line))) > 0) {
        struct Course course;
        char *token = strtok(line, ",");
        if (!token) continue;
        course.id = atoi(token);
        token = strtok(NULL, ",");
        if (token) strcpy(course.code, token);
        token = strtok(NULL, ",");
        if (token) strcpy(course.name, token);
        token = strtok(NULL, ",");
        if (token) course.capacity = atoi(token);
        token = strtok(NULL, ",");
        if (token) course.enrolled = atoi(token);
        token = strtok(NULL, ",");
        if (token) course.credits = atoi(token);
        token = strtok(NULL, ",");
        if (token) course.faculty_id = atoi(token);

        // Skip if already enrolled or full
        if ((strlen(student.enrolled_courses) > 0 && strstr(student.enrolled_courses, course.code)) || 
            course.enrolled >= course.capacity) {
            continue;
        }

        // Get faculty name
        char faculty_name[100] = "Unknown";
        int faculty_fd = open(DB_FACULTY, O_RDONLY);
        if (faculty_fd >= 0) {
            struct flock fac_lock;
            memset(&fac_lock, 0, sizeof(fac_lock));
            fac_lock.l_type = F_RDLCK;
            fac_lock.l_whence = SEEK_SET;
            fac_lock.l_start = 0;
            fac_lock.l_len = 0;
            
            if (fcntl(faculty_fd, F_SETLKW, &fac_lock) == -1) {
                const char *err_msg = "Failed to acquire lock on faculty file\n";
                write(STDERR_FILENO, err_msg, strlen(err_msg));
            } else {
                char faculty_line[MAX_LINE];
                while (read_line(faculty_fd, faculty_line, sizeof(faculty_line)) > 0) {
                    int fid;
                    char *ftoken = strtok(faculty_line, ",");
                    if (!ftoken) continue;
                    fid = atoi(ftoken);
                    if (fid == course.faculty_id) {
                        ftoken = strtok(NULL, ",");
                        if (ftoken) strcpy(faculty_name, ftoken);
                        break;
                    }
                }
                
                // Release lock on faculty file
                fac_lock.l_type = F_UNLCK;
                fcntl(faculty_fd, F_SETLK, &fac_lock);
            }
            close(faculty_fd);
        }

        char course_line[256];
        int len = snprintf(course_line, sizeof(course_line),
                          "\n║ %-9d ║ %-9s ║ %-9d ║ %-30s ║ %-19s ║",
                          course.id, course.code, course.credits, course.name, faculty_name);
        write(STDOUT_FILENO, course_line, len);
        count++;
    }

    if (count == 0) {
        const char *no_courses = "\n║                      No available courses for enrollment at this time                     ║";
        write(STDOUT_FILENO, no_courses, strlen(no_courses));
    }

    const char *footer = "\n╚═══════════╩═══════════╩═══════════╩════════════════════════════════╩═════════════════════╝\n";
    write(STDOUT_FILENO, footer, strlen(footer));

    // Release lock on course file
    lock.l_type = F_UNLCK;
    fcntl(course_fd, F_SETLK, &lock);
    close(course_fd);
    
    return SUCCESS;
}


/**
 * @brief Enrolls a student in a course, updating both course and student records.
 * 
 * @param student_id ID of the student.
 * @param course_id ID of the course.
 * @return int Status code (SUCCESS, ALREADY_ENROLLED, FILE_ERROR, etc).
 */
static inline int enroll_course(int student_id, int course_id) {
    // 1) Look up course code & increment enrolled count
    int cf = open(DB_COURSES, O_RDONLY);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on courses file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(cf);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    char course_code[64] = "";
    int found_course = 0;
    int cap = 0, enrolled = 0, credits = 0, fid = 0;
    char name_c[128] = "";
    char students_list[MAX_BUFFER] = "";  // Increased buffer size
    char updated_courses[MAX_COURSE_BUFFER] = ""; // Increased buffer size
    ssize_t bytes_read;

    // preserve header
    if ((bytes_read = read_line(cf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3) == 0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_courses, line);
        } else {
            strcat(updated_courses, line);
            strcat(updated_courses, "\n");
        }
    } else {
        lseek(cf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(cf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';
        students_list[0] = '\0';  // Reset for each line

        int cid;
        int fields = sscanf(line,
            "%d,%63[^,],%127[^,],%d,%d,%d,%d,\"%511[^\"]\"",
            &cid, course_code, name_c, &cap, &enrolled, &credits, &fid, students_list);

        if (cid == course_id) {
            found_course = 1;
            
            // Check if already enrolled
            if (fields == 8 && strlen(students_list)) {
                char temp_list[MAX_BUFFER]; // Increased buffer size
                strcpy(temp_list, students_list);
                char *tok = strtok(temp_list, ",");
                while (tok) {
                    if (atoi(tok) == student_id) {
                        // Release lock
                        lock.l_type = F_UNLCK;
                        fcntl(cf, F_SETLK, &lock);
                        close(cf);
                        return ALREADY_ENROLLED;
                    }
                    tok = strtok(NULL, ",");
                }
            }
            break;
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);
    
    if (!found_course) return COURSE_NOT_FOUND;

    // 2) Check if student exists
    int sf = open(DB_STUDENTS, O_RDONLY);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on students file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(sf);
        return FILE_ERROR;
    }

    int found_student = 0;
    char student_enrolled[MAX_BUFFER] = ""; // Increased buffer size
    
    while ((bytes_read = read_line(sf, line, sizeof(line))) > 0) {
        int sid;
        if (sscanf(line, "%d,%*[^,],%*[^,],%*[^,],%*d,%[^\n]", &sid, student_enrolled) >= 1) {
            if (sid == student_id) {
                found_student = 1;
                // Check if already enrolled in this course
                if (strstr(student_enrolled, course_code)) {
                    // Release lock
                    lock.l_type = F_UNLCK;
                    fcntl(sf, F_SETLK, &lock);
                    close(sf);
                    return ALREADY_ENROLLED;
                }
                break;
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);
    
    if (!found_student) return USER_NOT_FOUND;

    // 3) Update courses file
    cf = open(DB_COURSES, O_RDONLY);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on courses file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(cf);
        return FILE_ERROR;
    }

    updated_courses[0] = '\0';
    if ((bytes_read = read_line(cf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3) == 0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_courses, line);
        } else {
            strcat(updated_courses, line);
            strcat(updated_courses, "\n");
        }
    } else {
        lseek(cf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(cf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';

        int cid;
        if (sscanf(line, "%d", &cid) == 1) {
            if (cid == course_id) {
                char new_students[MAX_BUFFER]; // Increased buffer size
                if (strlen(students_list) > 0) {
                    // Use safer snprintf with sufficient buffer size
                    if (snprintf(new_students, sizeof(new_students), "%s,%d", students_list, student_id) >= (int)sizeof(new_students)) {
                        // Buffer would overflow, handle error
                        lock.l_type = F_UNLCK;
                        fcntl(cf, F_SETLK, &lock);
                        close(cf);
                        return FILE_ERROR;
                    }
                } else {
                    if (snprintf(new_students, sizeof(new_students), "%d", student_id) >= (int)sizeof(new_students)) {
                        // Buffer would overflow, handle error
                        lock.l_type = F_UNLCK;
                        fcntl(cf, F_SETLK, &lock);
                        close(cf);
                        return FILE_ERROR;
                    }
                }
                char buf[MAX_COURSE_BUFFER]; // Increased buffer size
                if (snprintf(buf, sizeof(buf),
                    "%d,%s,%s,%d,%d,%d,%d,\"%s\"\n",
                    cid, course_code, name_c, cap, enrolled+1, credits, fid, new_students) >= (int)sizeof(buf)) {
                    // Buffer would overflow, handle error
                    lock.l_type = F_UNLCK;
                    fcntl(cf, F_SETLK, &lock);
                    close(cf);
                    return FILE_ERROR;
                }
                strcat(updated_courses, buf);
            } else {
                strcat(updated_courses, line);
                strcat(updated_courses, "\n");
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);

    // Write back with exclusive lock
    cf = open(DB_COURSES, O_WRONLY | O_TRUNC);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire exclusive lock for writing
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on courses file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(cf);
        return FILE_ERROR;
    }
    
    write(cf, updated_courses, strlen(updated_courses));
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);

    // 4) Update students file
    sf = open(DB_STUDENTS, O_RDONLY);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on students file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(sf);
        return FILE_ERROR;
    }

    char updated_students[MAX_STUDENT_BUFFER] = ""; // Increased buffer size
    if ((bytes_read = read_line(sf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3) == 0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_students, line);
        } else {
            strcat(updated_students, line);
            strcat(updated_students, "\n");
        }
    } else {
        lseek(sf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(sf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';

        int sid, active;
        char name_s[64], email_s[64], pass[64], enrolled_s[MAX_BUFFER] = ""; // Increased buffer size
        if (sscanf(line, "%d,%[^,],%[^,],%[^,],%d,%[^\n]", &sid, name_s, email_s, pass, &active, enrolled_s) >= 5) {
            if (sid == student_id) {
                char new_ec[MAX_BUFFER]; // Increased buffer size
                if (strlen(enrolled_s)) {
                    // Use safer snprintf with sufficient buffer size
                    if (snprintf(new_ec, sizeof(new_ec), "%s,%s", enrolled_s, course_code) >= (int)sizeof(new_ec)) {
                        // Buffer would overflow, handle error
                        lock.l_type = F_UNLCK;
                        fcntl(sf, F_SETLK, &lock);
                        close(sf);
                        return FILE_ERROR;
                    }
                } else {
                    if (snprintf(new_ec, sizeof(new_ec), "%s", course_code) >= (int)sizeof(new_ec)) {
                        // Buffer would overflow, handle error
                        lock.l_type = F_UNLCK;
                        fcntl(sf, F_SETLK, &lock);
                        close(sf);
                        return FILE_ERROR;
                    }
                }
                char buf[MAX_STUDENT_BUFFER]; // Increased buffer size
                if (snprintf(buf, sizeof(buf),
                         "%d,%s,%s,%s,%d,%s\n",
                         sid, name_s, email_s, pass, active, new_ec) >= (int)sizeof(buf)) {
                    // Buffer would overflow, handle error
                    lock.l_type = F_UNLCK;
                    fcntl(sf, F_SETLK, &lock);
                    close(sf);
                    return FILE_ERROR;
                }
                strcat(updated_students, buf);
            } else {
                strcat(updated_students, line);
                strcat(updated_students, "\n");
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);

    // Write back with exclusive lock
    sf = open(DB_STUDENTS, O_WRONLY | O_TRUNC);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire exclusive lock for writing
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        const char *err_msg = "Failed to acquire lock on students file\n";
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        close(sf);
        return FILE_ERROR;
    }
    
    write(sf, updated_students, strlen(updated_students));
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);

    return SUCCESS;
}

/**
 * @brief Unenrolls a student from a course, updating both course and student records.
 * 
 * This function removes a student from a course's enrollment list and removes the course
 * from the student's enrolled courses list. It uses file locking to ensure data consistency
 * during the update operations.
 * 
 * @param student_id ID of the student.
 * @param course_id ID of the course.
 * @return int Status code (SUCCESS, NOT_ENROLLED, FILE_ERROR, etc).
 */
int unenroll_course(int student_id, int course_id) {
    // 1) Look up course code and check if student is enrolled
    int cf = open(DB_COURSES, O_RDONLY);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(cf);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    char course_code[64] = "";
    int found_course = 0;
    int cap, enrolled, credits, fid;
    char name_c[128], students_list[MAX_BUFFER] = ""; // Increased buffer size
    char updated_courses[MAX_COURSE_BUFFER] = ""; // Increased buffer size

    // preserve header
    ssize_t bytes_read;
    if ((bytes_read = read_line(cf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3)==0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_courses, line);
        } else {
            strcat(updated_courses, line);
            strcat(updated_courses, "\n");
        }
    } else {
        lseek(cf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(cf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';

        int cid;
        int fields = sscanf(line,
            "%d,%63[^,],%127[^,],%d,%d,%d,%d,\"%511[^\"]\"",
            &cid, course_code, name_c, &cap, &enrolled, &credits, &fid, students_list);

        if (cid == course_id) {
            found_course = 1;
            
            // Check if student is enrolled
            int student_found = 0;
            if (fields == 8 && strlen(students_list)) {
                char temp_list[MAX_BUFFER]; // Increased buffer size
                strcpy(temp_list, students_list);
                char *tok = strtok(temp_list, ",");
                while (tok) {
                    if (atoi(tok) == student_id) {
                        student_found = 1;
                        break;
                    }
                    tok = strtok(NULL, ",");
                }
            }
            if (!student_found) {
                // Release lock
                lock.l_type = F_UNLCK;
                fcntl(cf, F_SETLK, &lock);
                close(cf);
                return NOT_ENROLLED;
            }
            break;
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);
    
    if (!found_course) return COURSE_NOT_FOUND;

    // 2) Check if student exists and is enrolled in the course
    int sf = open(DB_STUDENTS, O_RDONLY);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(sf);
        return FILE_ERROR;
    }

    int found_student = 0;
    char student_enrolled[MAX_BUFFER] = ""; // Increased buffer size
    
    while ((bytes_read = read_line(sf, line, sizeof(line))) > 0) {
        int sid;
        if (sscanf(line, "%d,%*[^,],%*[^,],%*[^,],%*d,%[^\n]", &sid, student_enrolled) >= 1) {
            if (sid == student_id) {
                found_student = 1;
                // Check if enrolled in this course
                if (!strstr(student_enrolled, course_code)) {
                    // Release lock
                    lock.l_type = F_UNLCK;
                    fcntl(sf, F_SETLK, &lock);
                    close(sf);
                    return NOT_ENROLLED;
                }
                break;
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);
    
    if (!found_student) return USER_NOT_FOUND;

    // 3) Update courses file - remove student from course's students list
    cf = open(DB_COURSES, O_RDONLY);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(cf);
        return FILE_ERROR;
    }

    updated_courses[0] = '\0';
    if ((bytes_read = read_line(cf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3)==0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_courses, line);
        } else {
            strcat(updated_courses, line);
            strcat(updated_courses, "\n");
        }
    } else {
        lseek(cf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(cf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';

        int cid;
        if (sscanf(line, "%d", &cid) == 1) {
            if (cid == course_id) {
                // Remove student from students_list
                char new_students[MAX_BUFFER] = ""; // Increased buffer size
                char temp_list[MAX_BUFFER]; // Increased buffer size
                strcpy(temp_list, students_list);
                
                char *tok = strtok(temp_list, ",");
                int first = 1;
                while (tok) {
                    if (atoi(tok) != student_id) {
                        if (!first) {
                            strcat(new_students, ",");
                        }
                        strcat(new_students, tok);
                        first = 0;
                    }
                    tok = strtok(NULL, ",");
                }

                char buf[MAX_COURSE_BUFFER]; // Increased buffer size
                if (snprintf(buf, sizeof(buf),
                    "%d,%s,%s,%d,%d,%d,%d,\"%s\"\n",
                    cid, course_code, name_c, cap, enrolled-1, credits, fid, new_students) >= (int)sizeof(buf)) {
                    // Buffer would overflow, handle error
                    lock.l_type = F_UNLCK;
                    fcntl(cf, F_SETLK, &lock);
                    close(cf);
                    return FILE_ERROR;
                }
                strcat(updated_courses, buf);
            } else {
                strcat(updated_courses, line);
                strcat(updated_courses, "\n");
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);

    // Write back to courses file with exclusive lock
    cf = open(DB_COURSES, O_WRONLY | O_TRUNC);
    if (cf < 0) return FILE_ERROR;
    
    // Acquire exclusive lock for writing
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(cf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(cf);
        return FILE_ERROR;
    }
    
    write(cf, updated_courses, strlen(updated_courses));
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(cf, F_SETLK, &lock);
    close(cf);

    // 4) Update students file - remove course from student's enrolled courses
    sf = open(DB_STUDENTS, O_RDONLY);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire shared lock for reading
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(sf);
        return FILE_ERROR;
    }

    char updated_students[MAX_STUDENT_BUFFER] = ""; // Increased buffer size
    if ((bytes_read = read_line(sf, line, sizeof(line))) > 0 && strncmp(line, "id,", 3)==0) {
        // Check if the line already ends with a newline
        if (line[bytes_read-1] == '\n') {
            strcat(updated_students, line);
        } else {
            strcat(updated_students, line);
            strcat(updated_students, "\n");
        }
    } else {
        lseek(sf, 0, SEEK_SET);
    }

    while ((bytes_read = read_line(sf, line, sizeof(line))) > 0) {
        line[strcspn(line, "\r\n")] = '\0';

        int sid, active;
        char name_s[64], email_s[64], pass[64], enrolled_s[MAX_BUFFER] = ""; // Increased buffer size
        if (sscanf(line, "%d,%[^,],%[^,],%[^,],%d,%[^\n]", &sid, name_s, email_s, pass, &active, enrolled_s) >= 5) {
            if (sid == student_id) {
                // Remove course from enrolled courses
                char new_enrolled[MAX_BUFFER] = ""; // Increased buffer size
                char temp_ec[MAX_BUFFER]; // Increased buffer size
                strcpy(temp_ec, enrolled_s);
                
                char *tok = strtok(temp_ec, ",");
                int first = 1;
                while (tok) {
                    if (strcmp(tok, course_code) != 0) {
                        if (!first) {
                            strcat(new_enrolled, ",");
                        }
                        strcat(new_enrolled, tok);
                        first = 0;
                    }
                    tok = strtok(NULL, ",");
                }

                char buf[MAX_STUDENT_BUFFER]; // Increased buffer size
                if (snprintf(buf, sizeof(buf),
                         "%d,%s,%s,%s,%d,%s\n",
                         sid, name_s, email_s, pass, active, new_enrolled) >= (int)sizeof(buf)) {
                    // Buffer would overflow, handle error
                    lock.l_type = F_UNLCK;
                    fcntl(sf, F_SETLK, &lock);
                    close(sf);
                    return FILE_ERROR;
                }
                strcat(updated_students, buf);
            } else {
                strcat(updated_students, line);
                strcat(updated_students, "\n");
            }
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);

    // Write back to students file with exclusive lock
    sf = open(DB_STUDENTS, O_WRONLY | O_TRUNC);
    if (sf < 0) return FILE_ERROR;
    
    // Acquire exclusive lock for writing
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(sf, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(sf);
        return FILE_ERROR;
    }
    
    write(sf, updated_students, strlen(updated_students));
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(sf, F_SETLK, &lock);
    close(sf);

    return SUCCESS;
}

/**
 * @brief Displays all courses the student is currently enrolled in, formatted as a table.
 * 
 * @param student_id ID of the student.
 * @return int SUCCESS on success, or FILE_ERROR / USER_NOT_FOUND.
 */
static inline int view_enrollments_st(int student_id) {
    FILE *student_fp = fopen(DB_STUDENTS, "r");
    if (!student_fp) {
        perror("Error opening students file");
        return FILE_ERROR;
    }
    int student_fp_fd = fileno(student_fp);
    if (flock(student_fp_fd, LOCK_SH) == -1) {
        perror("flock");
        fclose(student_fp);
        return FILE_ERROR;
    }

    char line[MAX_LINE];
    Student student;
    int found = 0;
    student.enrolled_courses[0] = '\0'; // Initialize to empty string

    // Step 1: Find student and extract enrolled course codes
    while (fgets(line, sizeof(line), student_fp)) {
        int sid;
        char name[100], email[100], password[100], enrolled_courses[MAX_BUFFER] = ""; // Initialize to empty
        int active;

        if (sscanf(line, "%d,%[^,],%[^,],%[^,],%d,%[^\n]", &sid, name, email, password, &active, enrolled_courses) >= 5) {
            if (sid == student_id) {
                strcpy(student.name, name);
                strcpy(student.enrolled_courses, enrolled_courses);
                found = 1;
                break;
            }
        }
    }
    flock(student_fp_fd, LOCK_UN);
    fclose(student_fp);

    if (!found) {
        printf("\n╔════════════════════════════════════════════════╗");
        printf("\n║ Error: Student with ID %d not found in system. ║", student_id);
        printf("\n╚════════════════════════════════════════════════╝\n");
        return USER_NOT_FOUND;
    }

    // Step 2: Split enrolled_courses into individual course codes
    char *tokens[64];
    int course_count = 0;
    
    // Only process enrolled courses if the string is not empty
    if (student.enrolled_courses[0] != '\0') {
        char *token = strtok(student.enrolled_courses, ",");
        while (token && course_count < 64) {
            tokens[course_count++] = strdup(token);
            token = strtok(NULL, ",");
        }
    }

    if (course_count == 0) {
        printf("\n╔═══════════════════════════════════════════════════════════╗");
        printf("\n║ You are not currently enrolled in any courses.            ║");
        printf("\n║ Use the 'Enroll in Course' option to register for classes.║");
        printf("\n╚═══════════════════════════════════════════════════════════╝\n");
        return SUCCESS;
    }

    FILE *courses_fp = fopen(DB_COURSES, "r");
    if (!courses_fp) {
        perror("Error opening courses file");
        // Free allocated memory before returning
        for (int i = 0; i < course_count; ++i) {
            free(tokens[i]);
        }
        return FILE_ERROR;
    }
    int courses_fp_fd = fileno(courses_fp);
    if (flock(courses_fp_fd, LOCK_SH) == -1) {
        perror("flock");
        fclose(courses_fp);
        // Free allocated memory before returning
        for (int i = 0; i < course_count; ++i) {
            free(tokens[i]);
        }
        return FILE_ERROR;
    }

    printf("\n╔═══════════════════════════════════════════════════════════════════════════════════════════╗");
    printf("\n║                                  YOUR ENROLLED COURSES                                    ║");
    printf("\n╠═══════════╦═══════════╦════════════════════════════════╦═══════════╦═════════════════════╣");
    printf("\n║ Course ID ║   Code    ║          Course Name           ║  Credits  ║       Faculty       ║");
    printf("\n╠═══════════╬═══════════╬════════════════════════════════╬═══════════╬═════════════════════╣");

    int found_courses = 0;
    while (fgets(line, sizeof(line), courses_fp)) {
        int id, capacity, enrolled, credits, f_id;
        char code[16], cname[128], students_list[MAX_BUFFER]; // Increased buffer size

        if (sscanf(line, "%d,%[^,],%[^,],%d,%d,%d,%d,\"%[^\"]\"",
                   &id, code, cname, &capacity, &enrolled, &credits, &f_id, students_list) >= 7) {

            for (int i = 0; i < course_count; ++i) {
                if (strcmp(code, tokens[i]) == 0) {
                    found_courses++;
                    // Look up faculty name
                    char faculty_name[100] = "Unknown";
                    FILE *fac_fp = fopen(DB_FACULTY, "r");
                    if (fac_fp) {
                        int fac_fp_fd = fileno(fac_fp);
                        if (flock(fac_fp_fd, LOCK_SH) == -1) {
                            perror("flock");
                            fclose(fac_fp);
                        } else {
                            char fline[MAX_LINE];
                            while (fgets(fline, sizeof(fline), fac_fp)) {
                                int fid;
                                char *ftok = strtok(fline, ",");
                                if (ftok) fid = atoi(ftok);
                                if (fid == f_id) {
                                    ftok = strtok(NULL, ",");
                                    if (ftok) strcpy(faculty_name, ftok);
                                    break;
                                }
                            }
                            flock(fac_fp_fd, LOCK_UN);
                        }
                        fclose(fac_fp);
                    }

                    printf("\n║ %-9d ║ %-9s ║ %-30s ║ %-9d ║ %-19s ║", 
                           id, code, cname, credits, faculty_name);
                    break;
                }
            }
        }
    }

    // If no courses were found in the database, show a message
    if (found_courses == 0) {
        printf("\n║                      No matching courses found in the database                        ║");
    }

    printf("\n╚═══════════╩═══════════╩════════════════════════════════╩═══════════╩═════════════════════╝\n");

    flock(courses_fp_fd, LOCK_UN);
    fclose(courses_fp);

    for (int i = 0; i < course_count; ++i)
        free(tokens[i]);

    return SUCCESS;
}


/**
 * @brief Updates a student's password in the student database.
 *
 * This function reads the student database line by line, locks a line exclusively
 * when the student ID matches, updates the password, and then unlocks the line.
 *
 * @param student_id ID of the student to update
 * @param new_password New password for the student
 * @return SUCCESS on success, USER_NOT_FOUND if student not found, FILE_ERROR on failure
 */
static inline int change_student_password(int student_id, const char *new_password) {
    int fd = open(DB_STUDENTS, O_RDWR);
    if (fd < 0) return FILE_ERROR;

    char line[MAX_BUFFER]; // Increased buffer size
    off_t offset = 0;

    while (1) {
        ssize_t n = pread(fd, line, sizeof(line) - 1, offset);
        if (n <= 0) break;
        line[n] = '\0';
        char *nl = strchr(line, '\n');
        int linelen = nl ? (nl - line + 1) : n;

        int sid, active;
        char name[MAX_LINE], email[MAX_LINE], pass[MAX_LINE], courses[MAX_BUFFER]; // Increased buffer size

        if (sscanf(line, "%d,%[^,],%[^,],%[^,],%d,%[^\n]", &sid, name, email, pass, &active, courses) >= 5) {
            if (sid == student_id) {
                struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = offset, .l_len = linelen };
                if (fcntl(fd, F_SETLKW, &lock) < 0) {
                    close(fd);
                    return FILE_ERROR;
                }

                char updated_line[MAX_BUFFER]; // Increased buffer size
                // Check if the buffer is large enough
                int required_size = snprintf(NULL, 0, "%d,%s,%s,%s,%d,%s\n", 
                                           sid, name, email, new_password, active, courses);
                
                if (required_size >= (int)sizeof(updated_line)) {
                    // Buffer would overflow, handle error
                    lock.l_type = F_UNLCK;
                    fcntl(fd, F_SETLK, &lock);
                    close(fd);
                    return FILE_ERROR;
                }
                
                snprintf(updated_line, 2566,
                         "%d,%s,%s,%s,%d,%s\n", sid, name, email, new_password, active, courses);
                pwrite(fd, updated_line, strlen(updated_line), offset);

                lock.l_type = F_UNLCK;
                fcntl(fd, F_SETLK, &lock);
                close(fd);
                return SUCCESS;
            }
        }

        offset += linelen;
    }

    close(fd);
    return USER_NOT_FOUND;
}

#endif // STUDENT_ACTIONS_H
