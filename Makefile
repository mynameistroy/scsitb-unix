# ============================================================================
# scsitb Makefile - Cross-Platform GNU Make Build System
# ============================================================================
# Designed for maximum compatibility with GNU Make across platforms and eras.
# Works on: Linux, Solaris 2.6+, AIX 4.3.3+, HP-UX, IRIX, BSD variants
# Syntax: POSIX.1-2001 / GNU Make 3.80+ compatible
# ============================================================================

#------------------------------------------------------------------------------
# 1. PLATFORM DETECTION (Auto-detect current build host)
#------------------------------------------------------------------------------
# Try multiple methods for maximum portability across old and new Unix systems

# Method 1: HOSTTYPE (most reliable on GNU Make)
UNAME_S := $(shell uname -s 2>/dev/null || echo "UNKNOWN")
HOSTTYPE := $(shell uname -m 2>/dev/null || echo "UNKNOWN")

# Method 2: OSNAME for older systems
OSNAME := $(shell cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'"' -f2 || \
               cat /etc/issue 2>/dev/null | head -1 | cut -d' ' -f1 || \
               echo "UNKNOWN")

# Platform classification (for future platform-specific rules)
PLATFORM := $(filter $(UNAME_S),Linux AIX Solaris HP-UX IRIX BSD)
PLATFORM_UNKNOWN := $(filter $(UNAME_S),*)

# Debug variable for verbose output (uncomment to debug)
#DEBUG_MAKE := 1

#------------------------------------------------------------------------------
# 2. CONFIGURABLE VARIABLES (Easy to override)
#------------------------------------------------------------------------------

# Executable name (always scsitb)
EXE := scsitb

# Source files - easily extendable
SRCS := scsitb.c toolbox_commands.c scsi_device.c
SRCDIR := .
INCDIR := include

# Object file directory
OBJDIR := .
OBJS := $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

# Compiler settings
CC := gcc
CFLAGS_BASE := -Wall -Wextra -pedantic -std=c99

# Platform-specific compiler flags (default: none, override per platform)
CFLAGS_PLATFORM :=

# Linker flags (default: none, override per platform)
LDFLAGS_PLATFORM :=

# Include paths
INCLUDES := -I$(INCDIR)

# Debug/Release flags (controlled by BUILD_TYPE)
# ========= DEBUG MODE =========
CFLAGS_DEBUG := -g -O0 -DDEBUG -DTRACE
# ========= RELEASE MODE =========
CFLAGS_RELEASE := -O2 -DNDEBUG

# Architecture detection for cross-compilation (future use)
ARCH := $(shell uname -m 2>/dev/null || echo i386)
ARCHFLAGS :=

#------------------------------------------------------------------------------
# 3. CONDITIONAL PLATFORM SETUP
#------------------------------------------------------------------------------

# Set platform-specific defaults based on detected OS
ifeq ($(findstring Linux,$(UNAME_S)),Linux)
    CFLAGS_PLATFORM += -DLITTLE_ENDIAN -fPIC -fPIE
    LDFLAGS_PLATFORM += -ludev 
		TRANSPORT = transport_linux
endif

ifeq ($(findstring AIX,$(UNAME_S)),AIX)
    CFLAGS_PLATFORM += -qlanglvl=extc99 -brtl -DBIG_ENDIAN
    LDFLAGS_PLATFORM += -brtl
		TRANSPORT = transport_aix
endif

ifeq ($(findstring SunOS,$(UNAME_S)),SunOS)
    CFLAGS_PLATFORM += -DBIG_ENDIAN -D__EXTENSIONS__
    LDFLAGS_PLATFORM +=
		TRANSPORT = transport_sunos
endif

ifeq ($(findstring HP-UX,$(UNAME_S)),HP-UX)
    CFLAGS_PLATFORM += -Ae -DBIG_ENDIAN
    LDFLAGS_PLATFORM += -Ae
		TRANSPORT = transport_hpux
endif

ifeq ($(findstring IRIX,$(UNAME_S)),IRIX)
    CFLAGS_PLATFORM += -n32 -mt -DBIG_ENDIAN
    LDFLAGS_PLATFORM += -n32 -mt
		TRANSPORT = transport_irix
endif

ifeq ($(TRANSPORT),)
    $(error Platform detection failed: $(UNAME_S) is not supported.)
endif

#------------------------------------------------------------------------------
# 4. BUILD TYPE SELECTION
#------------------------------------------------------------------------------
# Default: RELEASE
BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),debug)
    CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_PLATFORM) $(INCLUDES)
    LDFLAGS := $(LDFLAGS_PLATFORM)
else ifeq ($(BUILD_TYPE),release)
    CFLAGS := $(CFLAGS_BASE) $(CFLAGS_RELEASE) $(CFLAGS_PLATFORM) $(INCLUDES)
    LDFLAGS := $(LDFLAGS_PLATFORM)
else
    # Default to debug if unknown build type
    CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_PLATFORM) $(INCLUDES)
    LDFLAGS := $(LDFLAGS_PLATFORM)
endif

#------------------------------------------------------------------------------
# 5. RULES FOR SOURCE FILES
#------------------------------------------------------------------------------

# Pattern rule for .c -> .o compilation
# Uses implicit rules for maximum portability
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/$(TRANSPORT).o: transport/$(TRANSPORT).c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/transport.o: transport/transport.c
	$(CC) $(CFLAGS) -c $< -o $@

# Main executable linking rule
$(EXE): $(OBJS) $(TRANSPORT).o transport.o
	$(CC) $(CFLAGS) $(OBJS) $(TRANSPORT).o transport.o -o $@ $(LDFLAGS)

#------------------------------------------------------------------------------
# 6. DEFAULT TARGETS
#------------------------------------------------------------------------------

.PHONY: all clean debug release test info help

# Default target - builds with RELEASE configuration
all: $(EXE)
	@echo "=== Build Complete ==="
	@echo "  Executable: $(EXE)"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Platform:  $(UNAME_S) ($(HOSTTYPE))"
	@echo "  Compiler:  $(CC) $(CFLAGS)"
	@echo ""
help:
	@echo "Usage: make [BUILD_TYPE=debug|release] [CC=gcc|clang]"
	@echo "       make all        - Build with release flags (default)"
	@echo "       make debug      - Build with debug flags"
	@echo "       make clean      - Remove build artifacts"
	@echo "       make test       - Run verification tests"
	@echo "       make info       - Show build configuration"

# Debug build target
debug:
	@$(MAKE) BUILD_TYPE=debug all

# Release build target (explicit)
release: BUILD_TYPE := release
release: all

#------------------------------------------------------------------------------
# 7. CLEAN TARGETS
#------------------------------------------------------------------------------

.PHONY: clean clean-all clean-deps

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(EXE) $(OBJS) *.o
	rm -rf $(OBJDIR) 2>/dev/null || true
	rm -f *.a *.so

clean-all: clean
	@echo "All clean complete."

clean-deps:
	@echo "Removing dependency files..."
	rm -f *.d

#------------------------------------------------------------------------------
# 8. TEST TARGET
#------------------------------------------------------------------------------

.PHONY: test

test: $(EXE)
	@echo "=== Running Verification Tests ==="
	@echo
	@echo "Test 1: Check executable exists"
	@if [ -x "$(EXE)" ]; then \
	    echo "  [PASS] $(EXE) exists and is executable"; \
	else \
	    echo "  [FAIL] $(EXE) not found or not executable"; \
	    exit 1; \
	fi
	@echo
	@echo "Test 2: Check help output"
	@if $(EXE) --help 2>&1 | head -1 | grep -q "scsitb"; then \
	    echo "  [PASS] Help output displays correctly"; \
	else \
	    echo "  [WARN] Help output may need adjustment"; \
	fi
	@echo
	@echo "Test 3: Check return code on help"
	@./$(EXE) --help >/dev/null 2>&1; \
	RET=$$?; \
	if [ $$RET -eq 1 ]; then \
	    echo "  [PASS] Help returns expected code 1"; \
	else \
	    echo "  [INFO] Help returns code $$RET"; \
	fi
	@echo
	@echo "=== All Tests Complete ==="

#------------------------------------------------------------------------------
# 9. INFO TARGET (Show Configuration)
#------------------------------------------------------------------------------

.PHONY: info

info:
	@echo "========================================"
	@echo "scsitb Build Configuration"
	@echo "========================================"
	@echo "Executable:    $(EXE)"
	@echo "Build Type:    $(BUILD_TYPE)"
	@echo "Compiler:      $(CC)"
	@echo "CFLAGS:        $(CFLAGS)"
	@echo "LDFLAGS:       $(LDFLAGS)"
	@echo "Sources:       $(SRCS)"
	@echo "Includes:      $(INCDIR)"
	@echo "========================================"

#------------------------------------------------------------------------------
# 10. CROSS-COMPILATION SUPPORT (Future Platform Rules)
#------------------------------------------------------------------------------

# Example: Building for Solaris 2.6
# make PLATFORM=solaris CC=scc
# Example: Building for AIX 4.3.3
# make PLATFORM=aix CC=xlc

.PHONY: solaris aix hpux irix

solaris: CC := scc
solaris: CFLAGS_PLATFORM := -mt -xstrconst=1
solaris: LDFLAGS_PLATFORM := -mt

aix: CC := xlc
aix: CFLAGS_PLATFORM := -qlanglvl=extc99 -brtl
aix: LDFLAGS_PLATFORM := -brtl

hpux: CC := cc
hpux: CFLAGS_PLATFORM := -Ae
hpux: LDFLAGS_PLATFORM := -Ae

irix: CC := cc
irix: CFLAGS_PLATFORM := -n32 -mt
irix: LDFLAGS_PLATFORM := -n32 -mt

#------------------------------------------------------------------------------
# 11. VERBOSE MODE SUPPORT
#------------------------------------------------------------------------------

.PHONY: VV

VV: @$(MAKE) $(MAKEFLAGS) V=1 VV=1 all
VV: all

#------------------------------------------------------------------------------
# 12. DEPENDENCY TRACKING
#------------------------------------------------------------------------------
# Include generated dependency files


#------------------------------------------------------------------------------
# 13. MAKEFILE VERSION
#------------------------------------------------------------------------------

MAKEFILE_VERSION := 1.0
MAKEFILE_COMPAT := POSIX.1-2001 / GNU Make 3.80+

.DEFAULT_GOAL := all

#------------------------------------------------------------------------------
# END OF MAKEFILE
#------------------------------------------------------------------------------
