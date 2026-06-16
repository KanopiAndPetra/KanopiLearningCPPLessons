# Makefile for the multi-file Inventory lesson (Petra, 2026-06-16).
#
# How to use:
#   make            — build the program
#   make run        — build and run
#   make clean      — remove build artifacts
#   make rebuild    — clean + build
#   make asan       — build with AddressSanitizer
#   make verbose    — show every command as it runs
#
# Why a Makefile (and not just g++ on one line)?
#   - Incremental builds: make only rebuilds files whose source
#     has changed since the last build. The single-line
#     `g++ ... *.cpp` rebuilds everything every time.
#   - The dependency graph is explicit: box.o depends on box.h,
#     inventory.o depends on inventory.h AND box.h, main.o depends
#     on both headers. If you edit box.h, make knows to rebuild
#     box.o, inventory.o, AND main.o.
#   - Build flags live in one place. -Wall -Wextra -Wpedantic
#     applied consistently.

# ---- Configuration ----------------------------------------------------

# The compiler. Override with `make CXX=clang++`.
CXX      ?= g++

# C++ standard. C++17 is the cron default.
CXXSTD   ?= -std=c++17

# Warnings: strict, to catch the ODR / declaration issues that
# come up in multi-file builds.
WARNINGS ?= -Wall -Wextra -Wpedantic

# Optimization / debug: -O0 + -g is what the cron workflow uses.
OPTDEBUG ?= -O0 -g

# Where to put object files. Keeping them in a build/ subdir
# keeps the source tree clean.
BUILD    := build

# The final binary.
TARGET   := P-2026-06-16-inventory-multi-file

# ---- Source / object lists --------------------------------------------

# The main() translation unit.
MAIN_SRC := P-2026-06-16-inventory-multi-file.cpp

# The class implementation files (one per class — the lesson).
BOX_SRC     := box.cpp
INVENTORY_SRC := inventory.cpp

# Every .cpp becomes one .o. Object files go in build/.
SRCS := $(MAIN_SRC) $(BOX_SRC) $(INVENTORY_SRC)
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SRCS))

# Header dependencies. If a header changes, every translation unit
# that includes it must be rebuilt. main.cpp includes both headers;
# inventory.cpp includes box.h (transitively or directly).
ALL_DEPS := box.h inventory.h

# ---- Recipes ----------------------------------------------------------

# The first target is the default. `make` with no args runs this.
all: $(TARGET)

# Link the final binary from the three object files.
# $@ = the target (the binary name).
# $^ = all prerequisites (the .o files).
$(TARGET): $(OBJS) | $(BUILD)
	$(CXX) $(OPTDEBUG) -o $@ $^

# Pattern rule: any .cpp under the current dir builds to build/<name>.o.
# -c   = compile only, don't link
# -MMD = generate .d files (header dependencies) alongside .o files
# $<   = the first prerequisite (the .cpp file)
$(BUILD)/%.o: %.cpp $(ALL_DEPS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARNINGS) $(OPTDEBUG) -MMD -MP -c -o $@ $<

# Ensure the build directory exists before any .o lands in it.
$(BUILD):
	mkdir -p $(BUILD)

# Run the program.
run: $(TARGET)
	./$(TARGET)

# Build with AddressSanitizer. Useful for catching use-after-free
# and similar — though our trace lines mean the program is heavily
# I/O-bound, so ASan mostly tests for "did I forget a #include guard?"
asan: CXXFLAGS_ASAN := -fsanitize=address -fno-omit-frame-pointer
asan: clean
	$(CXX) $(CXXSTD) $(WARNINGS) $(OPTDEBUG) $(CXXFLAGS_ASAN) \
	    -o $(TARGET) $(SRCS)

# Show what would be built (dry run).
verbose:
	@$(MAKE) --no-print-directory V=1 all

# Remove all build artifacts. Note: this deletes the build/ dir
# and the binary; it does NOT touch source files or headers.
clean:
	rm -rf $(BUILD) $(TARGET)

# Force a full rebuild.
rebuild: clean all

# Auto-generated header-dependency files. `make` reads these
# to know when to rebuild a .o because a header changed.
-include $(OBJS:.o=.d)

# Phony targets don't correspond to files; always run them
# even if a file with that name exists.
.PHONY: all run asan verbose clean rebuild
