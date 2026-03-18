#!/bin/bash
PROJECT_NAME=${1:-$(basename "$PWD")}

# --- UNIVERSAL DYNAMIC DISCOVERY (Enhanced for Folders) ---
RAW_INCLUDES=$(grep -h "#include" *.c 2>/dev/null | tr -d '\r' | awk -F'[<">]' '{print $2}' | sed 's/\.h//g')

LDLIBS_AUTO=""

for entry in $RAW_INCLUDES; do
    lib=$(echo "$entry" | cut -d'/' -f1)
    echo "🔎 Checking: $lib"

    if pkg-config --exists "$lib" 2>/dev/null; then
        LDLIBS_AUTO="$LDLIBS_AUTO $(pkg-config --libs "$lib")"
    elif [[ "$lib" == "alsa" ]]; then
        echo "   -> Mapping alsa to -lasound"
        LDLIBS_AUTO="$LDLIBS_AUTO -lasound"
    elif [[ "$lib" == "raylib" ]]; then
        LDLIBS_AUTO="$LDLIBS_AUTO -lraylib -lGL -lm -lpthread -ldl -lrt -lX11"
    fi
done

grep -qi "math.h" *.c 2>/dev/null && LDLIBS_AUTO="$LDLIBS_AUTO -lm"
LDLIBS_STR=$(echo "$LDLIBS_AUTO" | xargs -n1 | sort -u | xargs)

# --- GENERATE MAKEFILE ---
cat <<EOF > makefile
CC = clang
BASE_FLAGS = -Wall -Wextra -MMD -MP
TARGET = $PROJECT_NAME
DEBUG_TARGET = \$(TARGET)_debug
RELEASE_TARGET = \$(TARGET)_release
LDLIBS = $LDLIBS_STR

# DYNAMIC FILE LISTS
SRCS = \$(wildcard *.c)
OBJS = \$(SRCS:.c=.o)
DEPS = \$(SRCS:.c=.d)

# DEFAULT TARGET: Standard Binary
all: CFLAGS = \$(BASE_FLAGS) -O2
all: clean_binaries clean_objs \$(TARGET)

# DEBUG TARGET: Debug Binary
debug: CFLAGS = \$(BASE_FLAGS) -g -O0
debug: clean_binaries clean_objs \$(DEBUG_TARGET)

# RELEASE TARGET: Lean and Mean
release: CFLAGS = \$(BASE_FLAGS) -O3 -march=native -flto
release: LDFLAGS = -s
release: clean_binaries clean_objs \$(RELEASE_TARGET)

# Rule for Standard Binary
\$(TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(OBJS) -o \$(TARGET) \$(LDLIBS)
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Standard Build Successful: '\$(TARGET)' preserved ---"

# Rule for Debug Binary
\$(DEBUG_TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(OBJS) -o \$(DEBUG_TARGET) \$(LDLIBS)
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Debug Build Successful: '\$(DEBUG_TARGET)' preserved ---"

# Rule for Release Binary
\$(RELEASE_TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(LDFLAGS) \$(OBJS) -o \$(RELEASE_TARGET) \$(LDLIBS)
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Release Build Successful: '\$(RELEASE_TARGET)' is lean and mean ---"

-include \$(DEPS)

%.o: %.c
	\$(CC) \$(CFLAGS) -c $< -o \$@

# CLEANUP
clean: clean_binaries clean_objs

clean_binaries:
	@rm -f \$(TARGET) \$(DEBUG_TARGET) \$(RELEASE_TARGET)

clean_objs:
	@rm -f \$(OBJS) \$(DEPS)

rebuild: clean all

.PHONY: all clean rebuild debug release clean_objs clean_binaries
EOF

echo "✅ Smart Makefile generated for $PROJECT_NAME"
echo "👉 Linker Flags: $LDLIBS_STR"
