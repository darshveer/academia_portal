#ifndef FACULTY_ACTIONS_H
#define FACULTY_ACTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "types.h"
#include "utils.h"

#define MAX_LINE_LEN 512

#define COURSE_DB   "../database/courses.csv"
#define STUDENT_DB  "../database/students.csv"
#define FACULTY_DB  "../database/faculty.csv"

// Add forward declaration of unenroll_course from student_actions.h
int unenroll_course(int student_id, int course_id);

/**
 * @brief Removes leading and trailing quotes from a string.
 * @param str Input string (modified in-place).
 */
static void strip_quotes(char *str) {
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

/**
 * @brief Checks if a course with the given ID exists.
 * @param id Course ID.
 * @return 1 if exists, 0 otherwise.
 */
static int check_course_id_exists(int id) {
    int fd = open(COURSE_DB, O_RDONLY);
    if (fd < 0) return 0;

    // Shared lock for reading
    struct flock lock = { .l_type = F_RDLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(fd);
        return 0;
    }

    char line[MAX_LINE_LEN];
    int exists = 0;

    while (read_line(fd, line, sizeof(line)) > 0) {
        int existing_id;
        if (sscanf(line, "%d,", &existing_id) == 1 && existing_id == id) {
            exists = 1;
            break;
        }
    }

    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return exists;
}

/**
 * @brief Updates the list of offered courses for a faculty.
 * @param faculty_id Faculty ID.
 * @param course_code Course code.
 * @param add 1 to add, 0 to remove the course.
 * @return SUCCESS, FILE_ERROR, or USER_NOT_FOUND.
 */
static int update_faculty_courses(int faculty_id, const char *course_code, int add) {
    char temp_file[] = "../database/faculty_temp.csv";
    int fd = open(FACULTY_DB, O_RDWR);
    if (fd < 0) return FILE_ERROR;

    // Exclusive lock for modification
    struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return FILE_ERROR;
    }

    FILE *faculty = fdopen(fd, "r+");
    FILE *temp = fopen(temp_file, "w");
    if (!faculty || !temp) {
        if (faculty) fclose(faculty);
        if (temp) fclose(temp);
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        return FILE_ERROR;
    }

    char line[MAX_LINE_LEN];
    int updated = 0;

    if (fgets(line, sizeof(line), faculty)) {
        fputs(line, temp);  // Copy header
    }

    while (fgets(line, sizeof(line), faculty)) {
        int id;
        char name[100], email[100], password[100], courses[256] = "";
        
        // Remove newline character if present
        line[strcspn(line, "\n")] = '\0';

        if (sscanf(line, "%d,%99[^,],%99[^,],%99[^,],%255[^\n]", 
                   &id, name, email, password, courses) >= 4) {
            if (id == faculty_id) {
                updated = 1;

                if (add) {
                    // Append course
                    if (strlen(courses) == 0) {
                        snprintf(courses, sizeof(courses), "%s", course_code);
                    } else {
                        strncat(courses, ",", sizeof(courses) - strlen(courses) - 1);
                        strncat(courses, course_code, sizeof(courses) - strlen(courses) - 1);
                    }
                } else {
                    // Remove course
                    char temp_courses[256] = "";
                    char *token = strtok(courses, ",");
                    int first = 1;
                    while (token) {
                        if (strcmp(token, course_code) != 0) {
                            if (!first) {
                                strcat(temp_courses, ",");
                            }
                            strcat(temp_courses, token);
                            first = 0;
                        }
                        token = strtok(NULL, ",");
                    }
                    strncpy(courses, temp_courses, sizeof(courses));
                }
            }

            // Write back updated line with newline
            fprintf(temp, "%d,%s,%s,%s,%s\n", id, name, email, password, courses);
        } else {
            // If line doesn't match expected format, write it as is with newline
            fprintf(temp, "%s\n", line);
        }
    }

    // Cleanup
    fclose(faculty);
    fclose(temp);
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    if (!updated) {
        remove(temp_file);
        return USER_NOT_FOUND;
    }

    // Replace old file
    if (rename(temp_file, FACULTY_DB) < 0) {
        remove(temp_file);
        return FILE_ERROR;
    }

    return SUCCESS;
}

/**
 * @brief Adds a new course to the system.
 * @param id Course ID.
 * @param code Course code.
 * @param name Course name.
 * @param capacity Max students.
 * @param credits Credit value.
 * @param faculty_id Owner faculty ID.
 * @return SUCCESS, DUPLICATE_ID, FILE_ERROR.
 */
int add_course(int id, const char *code, const char *name, int capacity, int credits, int faculty_id) {
    if (check_course_id_exists(id)) return DUPLICATE_ID;

    int fd = open(COURSE_DB, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return FILE_ERROR;

    // Lock file exclusively
    struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return FILE_ERROR;
    }

    FILE *course_file = fdopen(fd, "a+");
    if (!course_file) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        return FILE_ERROR;
    }

    fseek(course_file, 0, SEEK_END);
    if (ftell(course_file) == 0) {
        fprintf(course_file, "id,code,course_name,capacity,enrolled,credits,f_id,students\n");
    }

    fprintf(course_file, "%d,%s,%s,%d,0,%d,%d,\"\"\n", id, code, name, capacity, credits, faculty_id);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(course_file);

    return update_faculty_courses(faculty_id, code, 1);
}

/**
 * @brief Removes a course from the system.
 * @param id Course ID.
 * @param faculty_id Faculty who owns it.
 * @return SUCCESS, FILE_ERROR, NOT_FOUND.
 */


// Fix the remove_course function to remove unused variable and add proper return value
int remove_course(int id, int faculty_id) {
    char temp_file[] = "../database/courses_temp.csv";
    char course_code[32] = "";
    int found = 0;

    int fd = open(COURSE_DB, O_RDWR);
    if (fd < 0) return FILE_ERROR;

    struct flock lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return FILE_ERROR;
    }

    FILE *course_file = fdopen(fd, "r+");
    if (!course_file) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        return FILE_ERROR;
    }

    char line[MAX_LINE_LEN];
    char students_list[512] = "";
    
    // First, find the course and get its code and enrolled students
    if (fgets(line, sizeof(line), course_file)) {
        // Skip header
    }

    while (fgets(line, sizeof(line), course_file)) {
        int c_id, c_fid;
        if (sscanf(line, "%d,%31[^,],", &c_id, course_code) == 2 && c_id == id) {
            // Also check if this faculty is authorized to remove this course
            if (sscanf(line, "%*d,%*[^,],%*[^,],%*d,%*d,%*d,%d", &c_fid) == 1 && c_fid != faculty_id) {
                lock.l_type = F_UNLCK;
                fcntl(fd, F_SETLK, &lock);
                fclose(course_file);
                return NOT_FOUND; // Not authorized
            }
            
            found = 1;
            
            // Extract the students list
            char *students_start = strstr(line, "\"");
            if (students_start) {
                students_start++; // Skip the opening quote
                char *students_end = strstr(students_start, "\"");
                if (students_end) {
                    *students_end = '\0'; // Terminate the string at the closing quote
                    strcpy(students_list, students_start);
                }
            }
            break;
        }
    }

    // If course not found or not owned by this faculty, return error
    if (!found) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        fclose(course_file);
        return NOT_FOUND;
    }

    // Now directly update the students.csv file to remove the course code from all enrolled students
    if (strlen(students_list) > 0) {
        int students_fd = open(STUDENT_DB, O_RDWR);
        if (students_fd >= 0) {
            struct flock students_lock = { .l_type = F_WRLCK, .l_whence = SEEK_SET };
            if (fcntl(students_fd, F_SETLKW, &students_lock) != -1) {
                FILE *students_file = fdopen(students_fd, "r+");
                if (students_file) {
                    char temp_students_file[] = "../database/students_temp.csv";
                    FILE *temp_students = fopen(temp_students_file, "w");
                    
                    if (temp_students) {
                        char student_line[512];
                        // Copy header
                        if (fgets(student_line, sizeof(student_line), students_file)) {
                            fputs(student_line, temp_students);
                        }
                        
                        // Process each student
                        while (fgets(student_line, sizeof(student_line), students_file)) {
                            int student_id;
                            char name[100], email[100], password[100], enrolled_courses[512];
                            int active;
                            
                            // Remove newline if present
                            student_line[strcspn(student_line, "\n")] = '\0';
                            
                            if (sscanf(student_line, "%d,%[^,],%[^,],%[^,],%d,%[^\n]", 
                                      &student_id, name, email, password, &active, enrolled_courses) >= 5) {
                                
                                // Check if this student is enrolled in the course being removed
                                if (strstr(enrolled_courses, course_code)) {
                                    // Remove the course code from enrolled_courses
                                    char new_enrolled[512] = "";
                                    char *token = strtok(enrolled_courses, ",");
                                    int first = 1;
                                    
                                    while (token) {
                                        if (strcmp(token, course_code) != 0) {
                                            if (!first) strcat(new_enrolled, ",");
                                            strcat(new_enrolled, token);
                                            first = 0;
                                        }
                                        token = strtok(NULL, ",");
                                    }
                                    
                                    // Write updated student record
                                    fprintf(temp_students, "%d,%s,%s,%s,%d,%s\n", 
                                           student_id, name, email, password, active, new_enrolled);
                                } else {
                                    // Student not enrolled in this course, write unchanged
                                    fprintf(temp_students, "%s\n", student_line);
                                }
                            } else {
                                // Line doesn't match expected format, write as is
                                fprintf(temp_students, "%s\n", student_line);
                            }
                        }
                        
                        fclose(temp_students);
                        fclose(students_file);
                        
                        // Replace the original file with the temp file
                        rename(temp_students_file, STUDENT_DB);
                    } else {
                        fclose(students_file);
                    }
                }
                
                students_lock.l_type = F_UNLCK;
                fcntl(students_fd, F_SETLK, &students_lock);
            }
            close(students_fd);
        }
    }

    // Now proceed with course removal
    rewind(course_file);
    FILE *temp = fopen(temp_file, "w");
    if (!temp) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        fclose(course_file);
        return FILE_ERROR;
    }

    // Copy header
    if (fgets(line, sizeof(line), course_file)) {
        fputs(line, temp);
    }

    // Copy all lines except the one to be removed
    while (fgets(line, sizeof(line), course_file)) {
        int c_id;
        if (sscanf(line, "%d,", &c_id) == 1 && c_id == id) {
            continue; // Skip this line (remove the course)
        }
        fputs(line, temp);
    }

    fclose(course_file);
    fclose(temp);
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    if (rename(temp_file, COURSE_DB) < 0) {
        remove(temp_file);
        return FILE_ERROR;
    }

    return update_faculty_courses(faculty_id, course_code, 0);
}

/**
 * @brief Changes password for the faculty.
 * @param faculty_file Path to faculty CSV.
 * @param faculty_id Faculty ID.
 * @param newpass New password.
 * @return SUCCESS, NOT_FOUND, or FAILURE.
 */
int change_password(const char *faculty_file, int faculty_id, const char *newpass) {
    int fd = open(faculty_file, O_RDONLY);
    if (fd < 0) return FAILURE;

    struct flock lock = { .l_type = F_RDLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return FAILURE;
    }

    int temp_fd = open("../database/faculty_temp.csv", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd < 0) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        return FAILURE;
    }

    char line[MAX_LINE_LEN];
    int found = 0;

    while (read_line(fd, line, sizeof(line)) > 0) {
        if (strncmp(line, "id,", 3) == 0) {
            write(temp_fd, line, strlen(line));
            continue;
        }

        char copy[MAX_LINE_LEN];
        strcpy(copy, line);
        char *id = strtok(copy, ",");
        char *name = strtok(NULL, ",");
        char *email = strtok(NULL, ",");
        strtok(NULL, ","); // old password
        char *rest = strtok(NULL, "\n");

        if (atoi(id) == faculty_id) {
            char new_line[MAX_LINE_LEN];
            int len = rest ? snprintf(new_line, sizeof(new_line), "%s,%s,%s,%s,%s\n", id, name, email, newpass, rest)
                           : snprintf(new_line, sizeof(new_line), "%s,%s,%s,%s\n", id, name, email, newpass);
            write(temp_fd, new_line, len);
            found = 1;
        } else {
            write(temp_fd, line, strlen(line));
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    close(temp_fd);

    if (!found) {
        unlink("../database/faculty_temp.csv");
        return NOT_FOUND;
    }

    unlink(faculty_file);
    rename("../database/faculty_temp.csv", faculty_file);
    return SUCCESS;
}
// Fix the view_enrollments function to return a value and remove unused variable
/**
 * @brief Displays enrollments for a specific course taught by a faculty member.
 *
 * This function reads the course database to find the specified course,
 * then displays all students enrolled in that course in a formatted table.
 * It uses file locking to ensure data consistency during reads.
 *
 * @param faculty_id ID of the faculty member
 * @param selected_course_code Code of the course to view enrollments for
 * @return SUCCESS on success, FAILURE on error
 */
int view_enrollments(int faculty_id, const char *selected_course_code) {
    int courses_fd = open(COURSE_DB, O_RDONLY);
    if (courses_fd < 0) return FAILURE;
    
    // Acquire shared lock for reading
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock the entire file
    
    if (fcntl(courses_fd, F_SETLKW, &lock) == -1) {
        perror("fcntl lock");
        close(courses_fd);
        return FAILURE;
    }
    
    char line[MAX_LINE_LEN];
    ssize_t bytes_read;
    
    // Skip header line
    read_line(courses_fd, line, sizeof(line));
    
    while ((bytes_read = read_line(courses_fd, line, sizeof(line))) > 0) {
        int id, capacity, enrolled, credits, fid;
        char code[50], name[100], student_ids_raw[256];
        
        // Initialize student_ids_raw in case there's no enrolled list
        strcpy(student_ids_raw, "");
        
        int fields = sscanf(line, "%d,%49[^,],%99[^,],%d,%d,%d,%d,%255[^\n]",
                            &id, code, name, &capacity, &enrolled, &credits, &fid, student_ids_raw);
        
        if (fields < 7) continue;
        
        if (strcmp(code, selected_course_code) == 0 && fid == faculty_id) {
            const char *header1 = "\n╔═══════════════════════════════════════════════════════════════════════╗\n";
            write(STDOUT_FILENO, header1, strlen(header1));
            
            char course_info[256];
            int len = snprintf(course_info, sizeof(course_info), 
                              "║ Course: %-30s (%-8s)                     ║\n", name, code);
            write(STDOUT_FILENO, course_info, len);
            
            const char *header2 = "╠═══════════════════════════════════════════════════════════════════════╣\n"
                                 "║ Enrolled Students:                                                    ║\n";
            write(STDOUT_FILENO, header2, strlen(header2));
            
            if (fields < 8 || strlen(student_ids_raw) == 0) {
                const char *no_students = "║ No students enrolled.                                                ║\n"
                                         "╚═══════════════════════════════════════════════════════════════════════╝\n";
                write(STDOUT_FILENO, no_students, strlen(no_students));
                
                // Release lock
                lock.l_type = F_UNLCK;
                fcntl(courses_fd, F_SETLK, &lock);
                close(courses_fd);
                return SUCCESS;
            }
            
            // Strip quotes if present
            if (strlen(student_ids_raw) > 0) {
                strip_quotes(student_ids_raw);
            }
            
            if (strlen(student_ids_raw) == 0) {
                const char *no_students = "║ No students enrolled.                                                ║\n"
                                         "╚═══════════════════════════════════════════════════════════════════════╝\n";
                write(STDOUT_FILENO, no_students, strlen(no_students));
                
                // Release lock
                lock.l_type = F_UNLCK;
                fcntl(courses_fd, F_SETLK, &lock);
                close(courses_fd);
                return SUCCESS;
            }
            
            char student_list[256];
            strcpy(student_list, student_ids_raw);
            
            const char *table_header = "╠═══════════╦═══════════════════════════════════════════════════════════╣\n"
                                      "║ Student ID ║ Student Name                                             ║\n"
                                      "╠═══════════╬═══════════════════════════════════════════════════════════╣\n";
            write(STDOUT_FILENO, table_header, strlen(table_header));
            
            char *sid_token = strtok(student_list, ",");
            
            while (sid_token) {
                int students_fd = open(STUDENT_DB, O_RDONLY);
                if (students_fd < 0) {
                    // Release lock
                    lock.l_type = F_UNLCK;
                    fcntl(courses_fd, F_SETLK, &lock);
                    close(courses_fd);
                    return FAILURE;
                }
                
                // Acquire shared lock for reading students file
                struct flock stu_lock;
                memset(&stu_lock, 0, sizeof(stu_lock));
                stu_lock.l_type = F_RDLCK;
                stu_lock.l_whence = SEEK_SET;
                stu_lock.l_start = 0;
                stu_lock.l_len = 0; // Lock the entire file
                
                if (fcntl(students_fd, F_SETLKW, &stu_lock) == -1) {
                    perror("fcntl lock");
                    close(students_fd);
                    continue;
                }
                
                // Skip header
                char stu_line[MAX_LINE_LEN];
                read_line(students_fd, stu_line, sizeof(stu_line));
                
                int student_found = 0;
                while (read_line(students_fd, stu_line, sizeof(stu_line)) > 0) {
                    char sid[10], sname[100];
                    sscanf(stu_line, "%9[^,],%99[^,]", sid, sname);
                    
                    if (atoi(sid) == atoi(sid_token)) {
                        char student_row[256];
                        int len = snprintf(student_row, sizeof(student_row),
                                          "║ %-9s ║ %-55s ║\n", sid, sname);
                        write(STDOUT_FILENO, student_row, len);
                        student_found = 1;
                        break;
                    }
                }
                
                // Release lock on students file
                stu_lock.l_type = F_UNLCK;
                fcntl(students_fd, F_SETLK, &stu_lock);
                close(students_fd);
                
                if (!student_found) {
                    char unknown_student[256];
                    int len = snprintf(unknown_student, sizeof(unknown_student),
                                      "║ %-9s ║ %-55s ║\n", sid_token, "Unknown student");
                    write(STDOUT_FILENO, unknown_student, len);
                }
                
                sid_token = strtok(NULL, ",");
            }
            
            const char *table_footer = "╚═══════════╩═══════════════════════════════════════════════════════════╝\n";
            write(STDOUT_FILENO, table_footer, strlen(table_footer));
            
            // Release lock
            lock.l_type = F_UNLCK;
            fcntl(courses_fd, F_SETLK, &lock);
            close(courses_fd);
            return SUCCESS;
        }
    }
    
    // Release lock
    lock.l_type = F_UNLCK;
    fcntl(courses_fd, F_SETLK, &lock);
    close(courses_fd);
    
    // Course not found
    const char *not_found_msg = "\n╔═══════════════════════════════════════════════════════════════════════╗\n"
                               "║ Course not found or you are not authorized to view this course.        ║\n"
                               "╚═══════════════════════════════════════════════════════════════════════╝\n";
    write(STDOUT_FILENO, not_found_msg, strlen(not_found_msg));
    return FAILURE;
}
#endif // FACULTY_ACTIONS_H
