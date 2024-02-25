# Use .PHONY to explicitly declare non-file targets to ensure Make doesn't confuse them with actual files.
.PHONY: all compile install clean show

# Define directories for source files and installation.
# The `?=` operator assigns values to variables if they are not already defined.
SRC_DIRS ?= .
INSTALL_DIR ?= /boot/overlays

# Find all device tree source files (.dts) in the SRC_DIRS directory.
SRCS := $(shell find $(SRC_DIRS) -name "*.dts")

# Generate the names of the output device tree blob files (.dtbo) from the source file names.
OBJS := $(addsuffix .dtbo,$(basename $(SRCS)))

# The default target. Running 'make' with no arguments will execute this target.
# Here, it's set to run the 'install' target, which depends on 'compile'.
all: install

# The clean target is used to remove any generated files.
# The '@' before a command suppresses the default echo of the command.
clean:
	@rm -rf *.dtbo

# The install target depends on the 'compile' target.
# It copies the compiled device tree overlay files (.dtbo) to the INSTALL_DIR.
# Using 'sudo' to ensure we have the necessary permissions for copying.
install: compile
	@sudo cp -v $(OBJS) $(INSTALL_DIR)

# The show target is a utility for displaying the currently active device tree structure.
# It uses the Device Tree Compiler (dtc) to read from the live device tree filesystem.
show:
	@dtc -I fs /sys/firmware/devicetree/base

# The compile target depends on the output object files (.dtbo).
# This is where the dependency files are actually built, so it doesn't need to do anything itself.
# The work is done in the pattern rule below.
compile: $(OBJS)

# Pattern rule for compiling device tree source files (.dts) into blob files (.dtbo).
# The '%' acts as a wildcard matching any file name, and '$<' refers to the first dependency (the source file).
# The dtc command is used for the compilation, with options specified for input and output formats.
%.dtbo: %.dts
	@dtc -W no-unit_address_vs_reg -I dts -O dtb -o $@ $<

