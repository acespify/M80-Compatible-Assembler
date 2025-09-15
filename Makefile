# Makefile for the 8080 Cross-Assembler

# --- Variables ---
# The final executable will be placed in the build/ directory.
TARGET := build/ay-m80

# The C++ compiler and its flags. -pthread has been removed as it's not needed.
CXX      := g++
CXXFLAGS := -std=c++17 -g -Wall -Wextra

# The directory where intermediate object files will be stored.
BUILD_DIR := build/obj

# --- Source Files ---
# Explicitly list the application's C++ source files.
APP_SRCS := \
    src/main.cpp \
    src/Assembler.cpp

# The full list of source files to be compiled.
SRCS := $(APP_SRCS)

# Automatically generate the list of object file paths, preserving the
# directory structure inside the BUILD_DIR.
# e.g., src/main.cpp becomes build/obj/src/main.o
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))


# --- Includes ---
# Add the path to our header files.
INCLUDES := -Iinclude


# --- Rules ---

# The default 'all' target, which depends on the final executable.
all: $(TARGET)

# Rule to link the final executable from all the compiled object files.
$(TARGET): $(OBJECTS)
	@echo "==> Linking executable..."
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ -static-libgcc -static-libstdc++
	@echo "==> Build finished successfully: $(TARGET)"

# Generic rule to compile any .cpp file into its corresponding .o file
# within the BUILD_DIR, maintaining the source folder structure.
$(BUILD_DIR)/%.o: %.cpp
	@echo "==> Compiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

# Rule to clean up all build artifacts.
clean:
	@echo "==> Cleaning build files..."
	rm -rf build

# Declare targets that are not actual files.
.PHONY: all clean
