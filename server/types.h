#ifndef TYPES_H
#define TYPES_H

#define PORT 8080

#define MAX_USERS                100
#define MAX_NAME_LEN            100
#define MAX_EMAIL_LEN           100
#define MAX_PASS_LEN             64
#define MAX_COURSES_PER_FACULTY 10
#define MAX_COURSES_PER_STUDENT  8
#define MAX_COURSE_CODE_LEN       8
#define MAX_COURSE_NAME_LEN     100

/**
 * @brief Role identifiers used for authentication and access control.
 */
enum Role {
    ADMIN = 1,
    STUDENT,
    FACULTY
};

/**
 * @brief Typedef for course codes.
 */
typedef char CourseCode[MAX_COURSE_CODE_LEN];

/**
 * @brief Structure to store faculty details.
 */
typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char password[MAX_PASS_LEN];
    CourseCode offered_courses[MAX_COURSES_PER_FACULTY]; ///< Array of course codes
} Faculty;

/**
 * @brief Structure to store student details.
 */
typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char password[MAX_PASS_LEN];
    int active;                            ///< 1 if active, 0 if deactivated
    char enrolled_courses[512];            ///< Comma-separated string of course codes
} Student;

/**
 * @brief Structure to represent a course.
 */
typedef struct Course {
    int id;
    char code[MAX_COURSE_CODE_LEN];
    char name[MAX_COURSE_NAME_LEN];
    int capacity;
    int enrolled;                          ///< Number of students currently enrolled
    int credits;
    int faculty_id;                        ///< Faculty ID who teaches the course
    Faculty *faculty;                      ///< Runtime pointer association (optional)
} Course;

/**
 * @brief Structure to store administrator credentials.
 */
typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char password[MAX_PASS_LEN];
} Admin;

#endif // TYPES_H
