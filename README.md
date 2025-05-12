# Student Registration System

This is a simple student registration system implemented in C. The system has three types of users: students, faculty, and administrators. Each user type has its own set of permissions and can perform certain actions.

## Modules

### `admin_actions.h`

This module contains functions for administrators to manage user accounts and perform other administrative tasks.

### `faculty_actions.h`

This module contains functions for faculty members to view their own details, view student details, and update their own details.

### `student_actions.h`

This module contains functions for students to view their own details, view faculty details, and update their own details.

### `utils.h`

This module contains utility functions for error handling, file I/O, and string manipulation.

### `types.h`

This module contains type definitions for the system.

### `server.c`

This is the main server program that listens for incoming connections and handles user requests.

### `client.c`

This is the main client program that connects to the server and provides a command-line interface for users to interact with the system.

## Stack

The following is a list of the technologies used in this project:

* C for the server and client
* TCP/IP for communication between the server and client

## How to build

To build the system, run the following command in the terminal:
```
make clean
make
```
To run the server:
```
./bin/server
```
To run the client:
```
./bin/client
```
