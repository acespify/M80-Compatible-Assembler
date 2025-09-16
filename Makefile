# Makefile for the ayM80 Cross-Assembler

# --- Variables ---
CXX      := g++
SRCDIR   := src
SOURCES  := $(wildcard $(SRCDIR)/*.cpp)

# Base compiler flags used for all builds
BASE_CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude

# --- Build Configurations ---
# Flags for a "debug" build: adds debug symbols (-g) and enables our debug macro
DEBUG_FLAGS   := -g -DDEBUG_MODE
# Flags for a "release" build: adds optimizations (-O2)
RELEASE_FLAGS := -O2

# --- Default Target ---
# The default 'all' target will create a release build.
.PHONY: all
all: release

# --- Build Rules ---

# Rule to create a release build in the build/release/ directory
.PHONY: release
release:
	$(MAKE) build_target \
	BUILD_DIR_NAME=release \
	CXXFLAGS="$(BASE_CXXFLAGS) $(RELEASE_FLAGS)"

# Rule to create a debug build in the build/debug/ directory
.PHONY: debug
debug:
	$(MAKE) build_target \
	BUILD_DIR_NAME=debug \
	CXXFLAGS="$(BASE_CXXFLAGS) $(DEBUG_FLAGS)"

# --- Internal Build Logic ---
# This is a generic target that the 'release' and 'debug' rules call.
# It uses the variables passed down from the rule that called it.

# Define output directories based on the build type (release or debug)
BUILD_DIR := build/$(BUILD_DIR_NAME)
OBJDIR    := $(BUILD_DIR)/obj
TARGET    := $(BUILD_DIR)/ayM80
OBJECTS   := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: build_target
build_target: $(TARGET)

# Generic Linking Rule
$(TARGET): $(OBJECTS)
	@echo "==> Linking $(BUILD_DIR_NAME) executable..."
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(OBJECTS) -o $@ -static-libgcc -static-libstdc++
	@echo "Build complete: $(TARGET) is ready."

# Generic Compilation Rule
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "==> Compiling $< for $(BUILD_DIR_NAME)..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean Target: Removes the entire build directory.
.PHONY: clean
clean:
	@echo "==> Cleaning all build files..."
	rm -rf build
