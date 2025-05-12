#ifndef STUDENT_CLIENT_H
#define STUDENT_CLIENT_H

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "../server/student_actions.h"
#include "../server/utils.h"

#define MAX_BUF 1024

/**
 * @brief Displays the student menu, gathers inputs, and invokes server functions.
 *
 * This function provides an interactive menu for the student to manage
 * courses and change passwords. It supports enrolling, unenrolling, viewing
 * enrolled courses, changing passwords, and exiting the menu.
 *
 * @param student_id The ID of the student using the menu.
 */
static inline void handle_student_menu(int student_id) {
    char buf[MAX_BUF];
    int choice;
    ssize_t n;

    while (1) {
        // 1) Print menu
        const char *menu =
            "\n---------------------------"
            "\n      STUDENT MENU       "
            "\n---------------------------"
            "\n 1) Enroll in Course     "
            "\n 2) Unenroll from Course "
            "\n 3) View Enrolled Courses"
            "\n 4) Change Password      "
            "\n 5) Exit                 "
            "\n---------------------------"
            "\nEnter choice: ";
        write(STDOUT_FILENO, menu, strlen(menu));

        // 2) Read choice line
        n = read(STDIN_FILENO, buf, MAX_BUF - 1);
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
            // Enroll
            int s = list_available_courses(student_id);
            if (s == USER_NOT_FOUND) {
                continue;
            }
            
            write(STDOUT_FILENO, "\nEnter course ID to enroll: ", 28);
            n = read(STDIN_FILENO, buf, MAX_BUF - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            int cid = atoi(buf);
            
            int res = enroll_course(student_id, cid);
            const char *msg;
            switch (res) {
                case SUCCESS:         
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Enrolled successfully!  ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case ALREADY_ENROLLED:
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Already enrolled!       ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case COURSE_NOT_FOUND:
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Course not found!       ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case USER_NOT_FOUND:  
                    msg = "\n╔═════════════════════════╗"
                          "\n║ User not found!         ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                default:              
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Enrollment failed!      ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
            }
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else if (choice == 2) {
            // Unenroll
            view_enrollments_st(student_id);
            write(STDOUT_FILENO, "\nEnter course ID to unenroll: ", 31);
            n = read(STDIN_FILENO, buf, MAX_BUF - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            int cid = atoi(buf);
            int res = unenroll_course(student_id, cid);
            const char *msg;
            switch (res) {
                case SUCCESS:        
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Unenrolled successfully!║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case USER_NOT_FOUND: 
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Enrollment not found!   ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case COURSE_NOT_FOUND: 
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Course not found!       ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                case FILE_ERROR:     
                    msg = "\n╔═════════════════════════╗"
                          "\n║ File error!             ║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
                default:             
                    msg = "\n╔═════════════════════════╗"
                          "\n║ Error during unenrollment!║"
                          "\n╚═════════════════════════╝\n"; 
                    break;
            }
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else if (choice == 3) {
            // View enrolled
            view_enrollments_st(student_id);
        }
        else if (choice == 4) {
            // Change password
            const char *prompt = "\n---------------------------"
                                 "\n    CHANGE PASSWORD      "
                                 "\n---------------------------"
                                 "\nEnter new password: ";
            write(STDOUT_FILENO, prompt, strlen(prompt));
            
            n = read(STDIN_FILENO, buf, MAX_BUF - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0'; // Remove newline
            
            int res = change_student_password(student_id, buf);
            const char *msg = (res == SUCCESS)
                ? "\n╔═════════════════════════╗"
                  "\n║   Password updated!     ║"
                  "\n╚═════════════════════════╝\n"
                : "\n╔═════════════════════════╗"
                  "\n║ Password update failed! ║"
                  "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else {
            const char *invalid_msg = "\n╔═════════════════════════╗"
                                     "\n║ Invalid option!         ║"
                                     "\n╚═════════════════════════╝\n";
            write(STDOUT_FILENO, invalid_msg, strlen(invalid_msg));
        }
    }
}

#endif // STUDENT_CLIENT_H
