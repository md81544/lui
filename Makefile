# Makefile - CMake wrapper
# App name extracted from the first add_executable() call in CMakeLists.txt.
# Usage:
#   make          -> debug build
#   make release  -> release build
#   make clean    -> remove all build directories and symlink

APP_NAME := lui

BUILD_DIR_DEBUG   := build/Debug
BUILD_DIR_RELEASE := build/Release

CMAKE       := cmake
CMAKE_FLAGS :=
JOBS        := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

.PHONY: all debug release clean

# ---------------------------------------------------------------------------
# Default target -> debug
# ---------------------------------------------------------------------------
all: debug

# ---------------------------------------------------------------------------
# Debug build
# ---------------------------------------------------------------------------
debug: $(BUILD_DIR_DEBUG)/Makefile
	$(CMAKE) --build $(BUILD_DIR_DEBUG) -- -j$(JOBS)

$(BUILD_DIR_DEBUG)/Makefile:
	@mkdir -p $(BUILD_DIR_DEBUG)
	$(CMAKE) -S . -B $(BUILD_DIR_DEBUG) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug $(CMAKE_FLAGS)

# ---------------------------------------------------------------------------
# Release build
# ---------------------------------------------------------------------------
release: $(BUILD_DIR_RELEASE)/Makefile
	$(CMAKE) --build $(BUILD_DIR_RELEASE) -- -j$(JOBS)

$(BUILD_DIR_RELEASE)/Makefile:
	@mkdir -p $(BUILD_DIR_RELEASE)
	$(CMAKE) -S . -B $(BUILD_DIR_RELEASE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS)

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------
clean:
	@rm -rf build $(APP_NAME)
	@echo "-> build directories and symlink removed"