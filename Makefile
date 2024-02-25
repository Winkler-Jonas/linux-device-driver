# Top-level Makefile for orchestrating builds across kernel_module, overlay, and user_space_app

.PHONY: all kernel_module overlay user_space_app clean install full_install

# Default target to build everything
all: kernel_module overlay user_space_app

# Target for building the kernel module
kernel_module:
	$(MAKE) -C kernel_module/

# Target for compiling the device-tree overlay
overlay:
	$(MAKE) -C overlay/

# Target for building the user-space application
user_space_app:
	$(MAKE) -C user_space_app/

# Target for cleaning up all build artifacts
clean:
	$(MAKE) -C kernel_module/ clean
	$(MAKE) -C overlay/ clean
	$(MAKE) -C user_space_app/ clean

# Target for installing all necessary components (if applicable)
install:
	$(MAKE) -C kernel_module/ modules_install
	$(MAKE) -C overlay/ install
	$(MAKE) -C user_space_app/ install INSTALL_DIR=$(PWD)

# Compile and install everything
full_install: all install

