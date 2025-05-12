# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -g
BIN_DIR = bin

# Directories
CLIENT_DIR = client
SERVER_DIR = server

# Main C source files (with main())
CLIENT_SRCS = $(CLIENT_DIR)/client.c

SERVER_SRC = $(SERVER_DIR)/server.c

# Output binary paths
CLIENT_BINS = $(CLIENT_SRCS:$(CLIENT_DIR)/%.c=$(BIN_DIR)/%)
SERVER_BIN = $(BIN_DIR)/server

# Build everything
all: $(BIN_DIR) $(SERVER_BIN) $(CLIENT_BINS)

# Ensure bin directory exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Server binary (links server.c which includes all headers)
$(SERVER_BIN): $(SERVER_SRC) $(SERVER_DIR)/types.h
	$(CC) $(CFLAGS) -o $@ $<

# Each client binary
$(BIN_DIR)/%: $(CLIENT_DIR)/%.c $(SERVER_DIR)/types.h
	$(CC) $(CFLAGS) -o $@ $<

# Clean all builds
clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
