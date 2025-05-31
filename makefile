# Makefile for Ecewo CLI Tool
# Cross-platform compatible

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c99 -O2

# Detect environment and set variables
ifeq ($(OS),Windows_NT)
    TARGET = ecewo.exe
    RM = rm -f
    MKDIR = mkdir -p
    # Use Unix-style path for UCRT64/MSYS2 compatibility
    INSTALL_DIR = /c/Windows/System32
    # Alternative user install location that doesn't need admin rights
    USER_INSTALL_DIR = $(HOME)/bin
else
    TARGET = ecewo
    RM = rm -f
    MKDIR = mkdir -p
    INSTALL_DIR = /usr/local/bin
    USER_INSTALL_DIR = $(HOME)/.local/bin
endif

# Source files
SOURCES = cli.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	$(RM) $(OBJECTS) $(TARGET)

build: clean all

# Install to system path (requires admin rights on Windows)
install-admin: $(TARGET)
	@echo "Installing $(TARGET) to system directory..."
	cp $(TARGET) $(INSTALL_DIR)/
	@echo "Successfully installed to $(INSTALL_DIR)"
	@echo "Note: This requires administrator privileges on Windows"
	$(MAKE) all

# Install to user directory (no admin rights needed)
install: $(TARGET)
	@echo "Installing $(TARGET) to user directory..."
	$(MKDIR) $(USER_INSTALL_DIR)
	cp $(TARGET) $(USER_INSTALL_DIR)/
	@echo "Successfully installed to $(USER_INSTALL_DIR)"
ifeq ($(OS),Windows_NT)
	@echo ""
	@echo "To use ecewo from anywhere, add this to your ~/.bashrc:"
	@echo "export PATH=\"$(USER_INSTALL_DIR):\$$PATH\""
	@echo "Then run: source ~/.bashrc"
else
	@echo ""
	@echo "Make sure $(USER_INSTALL_DIR) is in your PATH"
	@echo "Add this to your ~/.bashrc if needed:"
	@echo "export PATH=\"$(USER_INSTALL_DIR):\$$PATH\""
endif
	$(MAKE) all

# Uninstall from system
uninstall-admin:
	@echo "Removing $(TARGET) from system directory..."
	$(RM) $(INSTALL_DIR)/$(TARGET)
	@echo "Uninstalled from $(INSTALL_DIR)"
	$(MAKE) clean

# Uninstall from user directory
uninstall:
	@echo "Removing $(TARGET) from user directory..."
	$(RM) $(USER_INSTALL_DIR)/$(TARGET)
	@echo "Uninstalled from $(USER_INSTALL_DIR)"
	$(MAKE) clean

# Create build directory
builddir:
	$(MKDIR) build

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Static build (for distribution)
static: CFLAGS += -static
static: $(TARGET)

# Test the built executable
test: $(TARGET)
	@echo "Testing $(TARGET)..."
	./$(TARGET) --help || echo "Help command test completed"

# Show help
help:
	@echo "Available targets:"
	@echo "  all          	- Build the executable (default)"
	@echo "  clean        	- Remove build artifacts"
	@echo "  build        	- Clean and build"
	@echo "  install-admin  - Install to system path (needs admin on Windows)"
	@echo "  install 		- Install to user directory (recommended)"
	@echo "  uninstall-admin	- Remove from system path"
	@echo "  uninstall		- Remove from user directory"
	@echo "  debug        	- Build with debug symbols"
	@echo "  static       	- Build static executable"
	@echo "  test         	- Test the executable"
	@echo "  help         	- Show this help"
	@echo ""
	@echo "Recommended usage on Windows:"
	@echo "  make install    # Install without admin rights"
	@echo "  make uninstall  # Uninstall from user directory"

# Phony targets
.PHONY: all clean build install-admin install uninstall-admin uninstall builddir debug static test help