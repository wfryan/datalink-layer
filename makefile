# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Source files
COMMON = project_headers.h
PHYSICAL_SRC = physical.c
DATALINK_SRC = datalink.c
CLIENT_SRC = client.c
SERVER_SRC = server.c

# Object files
PHYSICAL_OBJ = physical.o
DATALINK_OBJ = datalink.o

# Output executables
CLIENT_EXEC = client
SERVER_EXEC = server

# Rules
all: $(CLIENT_EXEC) $(SERVER_EXEC)

$(PHYSICAL_OBJ): $(PHYSICAL_SRC) $(COMMON)
	$(CC) $(CFLAGS) -c $(PHYSICAL_SRC) -o $(PHYSICAL_OBJ)

$(DATALINK_OBJ): $(DATALINK_SRC) $(COMMON)
	$(CC) $(CFLAGS) -c $(DATALINK_SRC) -o $(DATALINK_OBJ)

$(CLIENT_EXEC): $(CLIENT_SRC) $(PHYSICAL_OBJ) $(DATALINK_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_SRC) $(PHYSICAL_OBJ) $(DATALINK_OBJ) -o $(CLIENT_EXEC)

$(SERVER_EXEC): $(SERVER_SRC) $(PHYSICAL_OBJ) $(DATALINK_OBJ)
	$(CC) $(CFLAGS) $(SERVER_SRC) $(PHYSICAL_OBJ) $(DATALINK_OBJ) -o $(SERVER_EXEC)

clean:
	rm -f $(CLIENT_EXEC) $(SERVER_EXEC) *.o
	make all
