#ifndef FACULTY_CLIENT_H
#define FACULTY_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../server/faculty_actions.h"
#include "../server/utils.h"

#define DB_COURSES "../database/courses.csv"
#define DB_STUDENTS "../database/students.csv"
#define DB_FACULTY "../database/faculty.csv"

/**
 * @brief Displays the faculty menu, gathers inputs, and invokes server functions.
 *
 * This function provides an interactive menu for the faculty to manage
 * courses and change passwords. It supports adding, removing, viewing
 * enrollments of courses, changing passwords, and exiting the menu.
 *
 * @param faculty_id The ID of the faculty using the menu.
 */
static inline void handle_faculty_menu(int faculty_id) {
    char buf[512];
    int choice;
    ssize_t n;

    while (1) {
        // Display menu and prompt for choice
        const char *menu =
            "\n---------------------------"
            "\n      FACULTY MENU       "
            "\n---------------------------"
            "\n 1) Add Course           "
            "\n 2) Remove Course        "
            "\n 3) View Enrollments     "
            "\n 4) Change Password      "
            "\n 5) Exit                 "
            "\n---------------------------"
            "\nEnter choice: ";
        write(STDOUT_FILENO, menu, strlen(menu));

        n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n <= 0) return;
        buf[n] = '\0';
        buf[strcspn(buf, "\n")] = '\0'; // Remove newline

        choice = atoi(buf);
        if (choice == 5) {
            const char *logout_msg = "\n╔═════════════════════════╗"
                                     "\n║      Logging out...     ║"
                                     "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, logout_msg, strlen(logout_msg));
            break;
        }

        if (choice == 1) {
            int id, capacity, credits;
            char code[MAX_COURSE_CODE_LEN], name[MAX_COURSE_NAME_LEN];

            const char *add_course_msg = "\n---------------------------"
                                         "\n      ADD NEW COURSE     "
                                         "\n---------------------------\n";
            write(STDOUT_FILENO, add_course_msg, strlen(add_course_msg));

            write(STDOUT_FILENO, "Enter course ID: ", 17);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            id = atoi(buf);

            write(STDOUT_FILENO, "Enter course code: ", 19);
            n = read(STDIN_FILENO, code, sizeof(code) - 1);
            if (n <= 0) continue;
            code[n] = '\0';
            code[strcspn(code, "\n")] = '\0'; // Remove newline

            write(STDOUT_FILENO, "Enter course name: ", 19);
            n = read(STDIN_FILENO, name, sizeof(name) - 1);
            if (n <= 0) continue;
            name[n] = '\0';
            name[strcspn(name, "\n")] = '\0'; // Remove newline

            write(STDOUT_FILENO, "Enter course capacity: ", 23);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            capacity = atoi(buf);

            write(STDOUT_FILENO, "Enter course credits: ", 22);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            credits = atoi(buf);

            // Add course to the database
            int result = add_course(id, code, name, capacity, credits, faculty_id);
            const char *msg;
            if (result == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║   Course added!         ║"
                      "\n╚═════════════════════════╝\n";
            else if (result == DUPLICATE_ID)
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error: ID already exists ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error adding course      ║"
                      "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        } else if (choice == 2) {
            int courses_fd = open(DB_COURSES, O_RDONLY);
            if (courses_fd < 0) {
                const char *err_msg = "\n╔═════════════════════════════════╗"
                                      "\n║ Could not open course database  ║"
                                      "\n╚═════════════════════════════════╝\n";
                write(STDOUT_FILENO, err_msg, strlen(err_msg));
                continue;
            }

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
                continue;
            }

            char line[512];
            int course_ids[50];
            char course_codes[50][32];
            char course_names[50][64];
            int course_count = 0;
            ssize_t bytes_read;

            // Skip header if present
            bytes_read = read_line(courses_fd, line, sizeof(line));
            if (bytes_read > 0 && strncmp(line, "id,", 3) != 0) {
                lseek(courses_fd, 0, SEEK_SET);
            }

            while ((bytes_read = read_line(courses_fd, line, sizeof(line))) > 0) {
                int id, cap, enr, cred, fid;
                char code[32], name[64];

                if (sscanf(line, "%d,%31[^,],%63[^,],%d,%d,%d,%d",
                           &id, code, name, &cap, &enr, &cred, &fid) == 7) {
                    if (fid == faculty_id) {
                        course_ids[course_count] = id;
                        strncpy(course_codes[course_count], code, sizeof(course_codes[0]));
                        strncpy(course_names[course_count], name, sizeof(course_names[0]));
                        course_count++;
                    }
                }
            }

            // Release lock
            lock.l_type = F_UNLCK;
            fcntl(courses_fd, F_SETLK, &lock);
            close(courses_fd);

            if (course_count == 0) {
                const char *no_courses_msg = "\n╔═════════════════════════════════╗"
                                             "\n║ You are not assigned to any     ║"
                                             "\n║ courses.                        ║"
                                             "\n╚═════════════════════════════════╝\n";
                write(STDOUT_FILENO, no_courses_msg, strlen(no_courses_msg));
                continue;
            }

            const char *list_msg = "\n-------------------------------------"
                                   "\n     YOUR OFFERED COURSES        "
                                   "\n-------------------------------------\n";
            write(STDOUT_FILENO, list_msg, strlen(list_msg));

            for (int i = 0; i < course_count; ++i) {
                char course_line[256];
                int len = snprintf(course_line, sizeof(course_line), 
                                  "%d. %-25s (%s)\n", i + 1, course_names[i], course_codes[i]);
                write(STDOUT_FILENO, course_line, len);
            }
            const char *end_list_msg = "-------------------------------------\n";
            write(STDOUT_FILENO, end_list_msg, strlen(end_list_msg));

            write(STDOUT_FILENO, "Enter number of the course to remove: ", 39);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            int sel = atoi(buf);

            if (sel < 1 || sel > course_count) {
                const char *invalid_sel_msg = "\n╔═════════════════════════╗"
                                              "\n║ Invalid selection!      ║"
                                              "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, invalid_sel_msg, strlen(invalid_sel_msg));
                continue;
            }

            int course_id = course_ids[sel - 1];
            int result = remove_course(course_id, faculty_id);
            const char *msg;
            if (result == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║   Course removed!       ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════════════╗"
                      "\n║ Error removing course or not    ║"
                      "\n║ authorized.                     ║"
                      "\n╚═════════════════════════════════╝\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        } else if (choice == 3) {
            int courses_fd = open(DB_COURSES, O_RDONLY);
            if (courses_fd < 0) {
                const char *err_msg = "\n╔═════════════════════════════════╗"
                                      "\n║ Could not open course database  ║"
                                      "\n╚═════════════════════════════════╝\n";
                write(STDOUT_FILENO, err_msg, strlen(err_msg));
                continue;
            }

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
                continue;
            }

            char line[512];
            char course_codes[10][32];
            char course_names[10][64];
            int course_count = 0;
            ssize_t bytes_read;

            // Skip header if present
            bytes_read = read_line(courses_fd, line, sizeof(line));
            if (bytes_read > 0 && strncmp(line, "id,", 3) != 0) {
                lseek(courses_fd, 0, SEEK_SET);
            }

            while ((bytes_read = read_line(courses_fd, line, sizeof(line))) > 0) {
                int id, cap, enr, cred, fid;
                char code[32], name[64];

                if (sscanf(line, "%d,%31[^,],%63[^,],%d,%d,%d,%d",
                           &id, code, name, &cap, &enr, &cred, &fid) == 7) {
                    if (fid == faculty_id) {
                        strncpy(course_codes[course_count], code, sizeof(course_codes[0]));
                        strncpy(course_names[course_count], name, sizeof(course_names[0]));
                        course_count++;
                    }
                }
            }

            // Release lock
            lock.l_type = F_UNLCK;
            fcntl(courses_fd, F_SETLK, &lock);
            close(courses_fd);

            if (course_count == 0) {
                const char *no_courses_msg = "\n╔═════════════════════════════════╗"
                                             "\n║ You are not assigned to any     ║"
                                             "\n║ courses.                        ║"
                                             "\n╚═════════════════════════════════╝\n";
                write(STDOUT_FILENO, no_courses_msg, strlen(no_courses_msg));
                continue;
            }

            const char *list_msg = "\n-------------------------------------"
                                   "\n     YOUR TEACHING COURSES       "
                                   "\n-------------------------------------\n";
            write(STDOUT_FILENO, list_msg, strlen(list_msg));

            for (int i = 0; i < course_count; i++) {
                char course_line[256];
                int len = snprintf(course_line, sizeof(course_line), 
                                  "%-2d. %-25s (%s)\n", i + 1, course_names[i], course_codes[i]);
                write(STDOUT_FILENO, course_line, len);
            }
            const char *end_list_msg = "\n-------------------------------------"
                                       "\n     END OF COURSE LIST       "
                                       "\n-------------------------------------\n";
            
            write(STDOUT_FILENO, end_list_msg, strlen(end_list_msg));

            write(STDOUT_FILENO, "Enter number of the course to view enrollments: ", 49);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            int sel = atoi(buf);

            if (sel < 1 || sel > course_count) {
                const char *invalid_sel_msg = "\n╔═════════════════════════╗"
                                              "\n║ Invalid selection!      ║"
                                              "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, invalid_sel_msg, strlen(invalid_sel_msg));
                continue;
            }

            view_enrollments(faculty_id, course_codes[sel - 1]);
        } else if (choice == 4) {
            char newpass[100];
            const char *change_pass_msg = "\n---------------------------"
                                          "\n    CHANGE PASSWORD      "
                                          "\n---------------------------\n";
            write(STDOUT_FILENO, change_pass_msg, strlen(change_pass_msg));
            write(STDOUT_FILENO, "Enter new password: ", 20);
            n = read(STDIN_FILENO, newpass, sizeof(newpass) - 1);
            if (n <= 0) continue;
            newpass[n] = '\0';
            newpass[strcspn(newpass, "\n")] = '\0'; // Remove newline

            // Change password in the database
            int result = change_password(DB_FACULTY, faculty_id, newpass);
            const char *msg;
            if (result == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║   Password updated!     ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ Failed to change password║"
                      "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        } else {
            const char *invalid_choice_msg = "\n╔═════════════════════════╗"
                                             "\n║ Invalid choice!         ║"
                                             "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, invalid_choice_msg, strlen(invalid_choice_msg));
        }
    }
}

#endif // FACULTY_CLIENT_H
