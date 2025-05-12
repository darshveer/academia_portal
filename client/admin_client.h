#ifndef ADMIN_CLIENT_H
#define ADMIN_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../server/admin_actions.h"
#include "../server/utils.h"

#define DB_STUDENTS "../database/students.csv"
#define DB_FACULTY  "../database/faculty.csv"

#define USER_NOT_FOUND -2
#define DUPLICATE_ID -3

/**
 * @brief Displays the admin menu, gathers inputs, and invokes server functions.
 *
 * This function provides an interactive menu for the admin to manage
 * student and faculty records. It supports adding new records, updating
 * user details, viewing user details, and exiting the menu.
 */
static inline void handle_admin_menu(void) {
    int choice;
    char buf[256];
    ssize_t n;

    while (1) {
        // Display menu and prompt for choice
        const char *menu =
            "\n---------------------------"
            "\n      ADMIN MENU         "
            "\n---------------------------"
            "\n 1) Add Student          "
            "\n 2) Add Faculty          "
            "\n 3) Update User Details  "
            "\n 4) View User Details    "
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
            // Add a new student
            int id, active;
            char name[100], email[100], pass[100];

            const char *add_student_msg = "\n---------------------------"
                                          "\n     ADD NEW STUDENT     "
                                          "\n---------------------------\n";
            write(STDOUT_FILENO, add_student_msg, strlen(add_student_msg));
            
            write(STDOUT_FILENO, "Enter student ID: ", 18);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            id = atoi(buf);
            
            write(STDOUT_FILENO, "Enter student name: ", 20);
            n = read(STDIN_FILENO, name, sizeof(name) - 1);
            if (n <= 0) continue;
            name[n] = '\0';
            name[strcspn(name, "\n")] = '\0';
            
            write(STDOUT_FILENO, "Enter email address: ", 21);
            n = read(STDIN_FILENO, email, sizeof(email) - 1);
            if (n <= 0) continue;
            email[n] = '\0';
            email[strcspn(email, "\n")] = '\0';
            
            write(STDOUT_FILENO, "Enter password: ", 16);
            n = read(STDIN_FILENO, pass, sizeof(pass) - 1);
            if (n <= 0) continue;
            pass[n] = '\0';
            pass[strcspn(pass, "\n")] = '\0';
            
            write(STDOUT_FILENO, "Enter active (1 = active, 0 = inactive): ", 41);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            active = atoi(buf);

            // Adding student with proper file locking
            int result = add_student(DB_STUDENTS, id, name, email, pass, active);
            const char *msg;
            if (result == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║   Student added!        ║"
                      "\n╚═════════════════════════╝\n";
            else if (result == DUPLICATE_ID)
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error: ID already exists ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error adding student     ║"
                      "\n╚═════════════════════════╝\n";
            
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else if (choice == 2) {
            // Add a new faculty
            int id;
            char name[100], email[100], pass[100];

            const char *add_faculty_msg = "\n---------------------------"
                                          "\n     ADD NEW FACULTY     "
                                          "\n---------------------------\n";
            write(STDOUT_FILENO, add_faculty_msg, strlen(add_faculty_msg));
            
            write(STDOUT_FILENO, "Enter faculty ID: ", 18);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            id = atoi(buf);
            
            write(STDOUT_FILENO, "Enter faculty name: ", 20);
            n = read(STDIN_FILENO, name, sizeof(name) - 1);
            if (n <= 0) continue;
            name[n] = '\0';
            name[strcspn(name, "\n")] = '\0';
            
            write(STDOUT_FILENO, "Enter email address: ", 21);
            n = read(STDIN_FILENO, email, sizeof(email) - 1);
            if (n <= 0) continue;
            email[n] = '\0';
            email[strcspn(email, "\n")] = '\0';
            
            write(STDOUT_FILENO, "Enter password: ", 16);
            n = read(STDIN_FILENO, pass, sizeof(pass) - 1);
            if (n <= 0) continue;
            pass[n] = '\0';
            pass[strcspn(pass, "\n")] = '\0';

            // Adding faculty with proper file locking
            int result = add_faculty(DB_FACULTY, id, name, email, pass);
            const char *msg;
            if (result == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║   Faculty added!        ║"
                      "\n╚═════════════════════════╝\n";
            else if (result == DUPLICATE_ID)
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error: ID already exists ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ Error adding faculty     ║"
                      "\n╚═════════════════════════╝\n";
            
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else if (choice == 3) {
            // Update user details
            char usertype;
            const char *update_msg = "\n---------------------------"
                                     "\n    UPDATE USER DETAILS  "
                                     "\n---------------------------\n";
            write(STDOUT_FILENO, update_msg, strlen(update_msg));
            
            write(STDOUT_FILENO, "Update Student or Faculty? (s/f): ", 35);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            usertype = buf[0];
        
            char db[256], newval[100];
            int field, user_id;
        
            if (usertype == 's' || usertype == 'S') {
                strcpy(db, DB_STUDENTS);
            } else {
                strcpy(db, DB_FACULTY);
            }
        
            // Show available users
            print_users(db);
        
            // Ask for user ID
            write(STDOUT_FILENO, "Enter user ID: ", 15);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            user_id = atoi(buf);
        
            const char *field_msg = "1) Update Name\n2) Update Email\n3) Update Password\n4) Toggle Active (only for students)\nEnter field: ";
            write(STDOUT_FILENO, field_msg, strlen(field_msg));
            
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            field = atoi(buf);
        
            if (field == 4 && usertype == 'f') {
                const char *error_msg = "\n╔═════════════════════════╗"
                                        "\n║ Invalid field choice    ║"
                                        "\n╚═════════════════════════╝\n";
                write(STDOUT_FILENO, error_msg, strlen(error_msg));
                continue;
            }

            if (field == 1 || field == 2 || field == 3) {
                write(STDOUT_FILENO, "Enter new value: ", 17);
                n = read(STDIN_FILENO, newval, sizeof(newval) - 1);
                if (n <= 0) continue;
                newval[n] = '\0';
                newval[strcspn(newval, "\n")] = '\0';
            } else {
                newval[0] = 0; // toggle active does not need newval
            }
        
            // Update user details with proper file locking
            int res = update_user_details(db, user_id, field, newval);
            const char *msg;
            if (res == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║ Updated successfully!   ║"
                      "\n╚═════════════════════════╝\n";
            else if (res == USER_NOT_FOUND)
                msg = "\n╔═════════════════════════╗"
                      "\n║ User not found          ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ Update failed           ║"
                      "\n╚═════════════════════════╝\n";
        
            write(STDOUT_FILENO, msg, strlen(msg));
        }
        else if (choice == 4) {
            // View user details
            char usertype;
            const char *view_msg = "\n---------------------------"
                                   "\n     VIEW USER DETAILS     "
                                   "\n---------------------------\n";
            write(STDOUT_FILENO, view_msg, strlen(view_msg));
            
            write(STDOUT_FILENO, "View Student or Faculty? (s/f): ", 33);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            usertype = buf[0];
        
            char db[256];
            int user_id;
        
            if (usertype == 's' || usertype == 'S') {
                strcpy(db, DB_STUDENTS);
            } else {
                strcpy(db, DB_FACULTY);
            }
        
            // Show available users
            print_users(db);
        
            // Ask for user ID
            write(STDOUT_FILENO, "Enter user ID: ", 15);
            n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) continue;
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            user_id = atoi(buf);
        
            // View user details with proper file locking
            int res = view_user_details(db, user_id);
            const char *msg;
            if (res == SUCCESS)
                msg = "\n╔═════════════════════════╗"
                      "\n║ User details displayed! ║"
                      "\n╚═════════════════════════╝\n";
            else if (res == USER_NOT_FOUND)
                msg = "\n╔═════════════════════════╗"
                      "\n║ User not found          ║"
                      "\n╚═════════════════════════╝\n";
            else
                msg = "\n╔═════════════════════════╗"
                      "\n║ View failed             ║"
                      "\n╚═════════════════════════╝\n";
        
            write(STDOUT_FILENO, msg, strlen(msg));
        }
    }
}

#endif // ADMIN_CLIENT_H

