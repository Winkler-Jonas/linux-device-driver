# Specify the default compiler to use
CC := gcc

# Compiler flags:
# -Wall: enables all compiler's warning messages
CFLAGS := -Wall

# Linker flags:
# -lpthread: links the POSIX thread library, necessary for using pthreads
LDFLAGS := -lpthread

# The name of the final executable to produce
TARGET := user_space_app

# List of object files that the application depends on.
# Each object file (*.o) corresponds to a source file (*.c)
OBJ := rainbow_hat.o user_space_app.o

# Installation directory can be overridden by passing INSTALL_DIR=<path> to make
INSTALL_DIR ?= .

# Phony targets are not associated with files
# Declaring 'all' and 'clean' as .PHONY means they are always executed when called
.PHONY: all clean install

# The default target built when you run 'make' with no arguments
# This target depends on the $(TARGET), so it will trigger building the $(TARGET)
all: $(TARGET)

# Linking the program
# The compiler $(CC) takes the object files $(OBJ), uses the linker flags $(LDFLAGS),
# and produces the final executable $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling source files into object files
# The -c flag tells the compiler to generate object code (.o files), not an executable
rainbow_hat.o: rainbow_hat.c rainbow_hat.h
	$(CC) $(CFLAGS) -c $<

user_space_app.o: user_space_app.c rainbow_hat.h
	$(CC) $(CFLAGS) -c $<

# Cleaning up
# This target removes the executable and object files
# 'rm -f' forces removal without prompting for confirmation and ignores nonexistent files
clean:
	rm -f $(TARGET) $(OBJ)

install:
	@echo "Installing the application to $(INSTALL_DIR)"
	@cp -v $(TARGET) $(INSTALL_DIR)/app

