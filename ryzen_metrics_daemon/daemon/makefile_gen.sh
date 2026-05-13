#!/bin/bash
# makefile_gen.sh - Universal Hybrid Version

# 1. DYNAMIC PROJECT DISCOVERY
# Extracts APPLICATION_NAME from /home/rob/Files/C/APPLICATION_NAME/source/
PROJECT_NAME=$(basename "$(dirname "$PWD")")
PKG_CONFIG=${PKG_CONFIG:-pkg-config}

echo "🛠️  Generating universal Makefile for: $PROJECT_NAME"

# --- UNIVERSAL DYNAMIC DISCOVERY ---
declare -A LIB_MAP=( ["sdl_mixer"]="SDL2_mixer" ["sdl_image"]="SDL2_image" ["sdl_ttf"]="SDL2_ttf" ["sdl"]="sdl2" ["asoundlib"]="alsa" )

# Scans source files to automatically link required libraries
RAW_INCLUDES=$(grep -h "#include" *.c 2>/dev/null | tr -d '\r' | awk -F'[<">]' '{print $2}' | sed 's/\.h//g')
LDLIBS_AUTO=""

for entry in $RAW_INCLUDES; do
    header=$(basename "$entry")
    lib_key=$(echo "$header" | tr '[:upper:]' '[:lower:]')
    lib=${LIB_MAP[$lib_key]:-$lib_key}

    if $PKG_CONFIG --exists "$lib" 2>/dev/null; then
        echo "  🔎 Linked: $lib"
        LDLIBS_AUTO="$LDLIBS_AUTO $($PKG_CONFIG --libs "$lib")"
    fi
done

grep -qi "math.h" *.c 2>/dev/null && LDLIBS_AUTO="$LDLIBS_AUTO -lm"

# Advanced String Cleanup (from MAY6): Deduplicate and strip trailing spaces
LDLIBS_STR=$(echo "$LDLIBS_AUTO" | tr ' ' '\n' | awk '!x[$0]++' | tr '\n' ' ' | sed 's/ $//')

# --- GENERATE MAKEFILE ---
cat <<EOF > makefile
# Use ?= to allow environment overrides for cross-compilation (from APR13)
CC ?= clang
PKG_CONFIG ?= pkg-config
BASE_FLAGS = -Wall -Wextra -MMD -MP
TARGET ?= $PROJECT_NAME
DEBUG_TARGET = \$(TARGET)_debug
RELEASE_TARGET = \$(TARGET)_release
LDLIBS = $LDLIBS_STR

# DYNAMIC FILE LISTS
SRCS = \$(wildcard *.c)
OBJS = \$(SRCS:.c=.o)
DEPS = \$(SRCS:.c=.d)

# DEFAULT TARGET: Standard Binary
all: CFLAGS = \$(BASE_FLAGS) -O2
all: clean_objs \$(TARGET)

# DEBUG TARGET
debug: CFLAGS = \$(BASE_FLAGS) -g -O0 -DDEBUG_MODE
debug: clean_objs \$(DEBUG_TARGET)

# RELEASE TARGET (Entry Point)
# Aggressive ISA override: Base x86-64 only, explicit AVX-512 disablement (from MAY6)
release: CFLAGS = \$(BASE_FLAGS) -O3 -march=x86-64 -mtune=generic -mno-avx512f
release: LDFLAGS = -s
release: clean_objs \$(RELEASE_TARGET)

# Rule for Standard Binary
\$(TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(OBJS) -o \$(TARGET) \$(LDLIBS)
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Standard Build Successful ---"

# Rule for Debug Binary
\$(DEBUG_TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(OBJS) -o \$(DEBUG_TARGET) \$(LDLIBS)
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Debug Build Successful ---"

# Rule for Release Binary (With Advanced ISA Metadata Removal)
\$(RELEASE_TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(LDFLAGS) \$(OBJS) -o \$(RELEASE_TARGET) \$(LDLIBS)
	@if command -v objcopy >/dev/null 2>&1; then \\
		echo "Nuking ISA metadata via objcopy..."; \\
		objcopy --remove-section=.note.gnu.property \$(RELEASE_TARGET); \\
	fi
	@if [ "\$(CC)" = \"clang\" ] || [ "\$(CC)" = \"gcc\" ]; then \\
		echo "Applying stealth strip..."; \\
		strip \$(RELEASE_TARGET) 2>/dev/null || true; \\
	fi
	@rm -f \$(OBJS) \$(DEPS)
	@echo "--- Release Build Successful: '\$(RELEASE_TARGET)' is ready ---"

-include \$(DEPS)

%.o: %.c
	\$(CC) \$(CFLAGS) -c \$< -o \$@

# CLEANUP
clean:
	rm -f \$(TARGET) \$(DEBUG_TARGET) \$(RELEASE_TARGET) \$(OBJS) \$(DEPS)

clean_objs:
	@rm -f \$(OBJS) \$(DEPS)

check:
	@echo "--- Running Static Analysis ---"
	cppcheck --enable=all --suppress=missingIncludeSystem --inconclusive --library=sdl2 ./

.PHONY: all clean rebuild debug release clean_objs check
EOF

echo "✅ Smart Makefile generated for $PROJECT_NAME"
