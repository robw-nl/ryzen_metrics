# Use ?= to allow environment overrides for cross-compilation (from APR13)
CC ?= clang
PKG_CONFIG ?= pkg-config
BASE_FLAGS = -Wall -Wextra -MMD -MP
TARGET ?= ryzen_metrics_daemon
DEBUG_TARGET = $(TARGET)_debug
RELEASE_TARGET = $(TARGET)_release
LDLIBS =  -lasound -lm

# DYNAMIC FILE LISTS
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# DEFAULT TARGET: Standard Binary
all: CFLAGS = $(BASE_FLAGS) -O2
all: clean_objs $(TARGET)

# DEBUG TARGET
debug: CFLAGS = $(BASE_FLAGS) -g -O0 -DDEBUG_MODE
debug: clean_objs $(DEBUG_TARGET)

# RELEASE TARGET (Entry Point)
# Aggressive ISA override: Base x86-64 only, explicit AVX-512 disablement (from MAY6)
release: CFLAGS = $(BASE_FLAGS) -O3 -march=x86-64 -mtune=generic -mno-avx512f
release: LDFLAGS = -s
release: clean_objs $(RELEASE_TARGET)

# Rule for Standard Binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDLIBS)
	@rm -f $(OBJS) $(DEPS)
	@echo "--- Standard Build Successful ---"

# Rule for Debug Binary
$(DEBUG_TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(DEBUG_TARGET) $(LDLIBS)
	@rm -f $(OBJS) $(DEPS)
	@echo "--- Debug Build Successful ---"

# Rule for Release Binary (With Advanced ISA Metadata Removal)
$(RELEASE_TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(RELEASE_TARGET) $(LDLIBS)
	@if command -v objcopy >/dev/null 2>&1; then \
		echo "Nuking ISA metadata via objcopy..."; \
		objcopy --remove-section=.note.gnu.property $(RELEASE_TARGET); \
	fi
	@if [ "$(CC)" = \"clang\" ] || [ "$(CC)" = \"gcc\" ]; then \
		echo "Applying stealth strip..."; \
		strip $(RELEASE_TARGET) 2>/dev/null || true; \
	fi
	@rm -f $(OBJS) $(DEPS)
	@echo "--- Release Build Successful: '$(RELEASE_TARGET)' is ready ---"

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# CLEANUP
clean:
	rm -f $(TARGET) $(DEBUG_TARGET) $(RELEASE_TARGET) $(OBJS) $(DEPS)

clean_objs:
	@rm -f $(OBJS) $(DEPS)

check:
	@echo "--- Running Static Analysis ---"
	cppcheck --enable=all --suppress=missingIncludeSystem --inconclusive --library=sdl2 ./

.PHONY: all clean rebuild debug release clean_objs check
