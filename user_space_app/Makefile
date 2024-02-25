# Makefile for compiling and linking rainbow_hat and user_space_app

# Compiler to use
CC=gcc

# Compiler flags
CFLAGS=-Wall

# Linker flags
LDFLAGS=-lpthread

# Name of the executable to produce
TARGET=user_space_app

# Object files from the source files
OBJ=rainbow_hat.o user_space_app.o

# Default target
all: $(TARGET)

# Rule to link the program
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile the source files into object files
# The -c flag tells the compiler to generate object code (.o files)
rainbow_hat.o: rainbow_hat.c rainbow_hat.h
	$(CC) $(CFLAGS) -c $<

user_space_app.o: user_space_app.c rainbow_hat.h
	$(CC) $(CFLAGS) -c $<

# Rule for cleaning up generated files
clean:
	rm -f $(TARGET) $(OBJ)

# Declare these targets as phony to avoid filename conflicts
.PHONY: all clean
