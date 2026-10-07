# scsitb build
# GNU make 3.75+ compatible: no ?=, no else-ifeq, no $(error), no patsubst,
# no .DEFAULT_GOAL. Platform settings live in config/<OS>.mk.

UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(PLATFORM),)
PLATFORM := $(UNAME_S)
endif

ifneq ($(wildcard config/$(PLATFORM).mk),)
include config/$(PLATFORM).mk
endif

EXE := scsitb
SRCS := scsitb.c toolbox_commands.c scsi_device.c
OBJS := $(SRCS:.c=.o)

ifeq ($(TRANSPORT),)
TRANSPORT_OBJS :=
else
TRANSPORT_OBJS := transport.o $(TRANSPORT).o
endif

ALL_OBJS := $(OBJS) $(TRANSPORT_OBJS)
HEADERS := $(wildcard include/*.h)
AVAILABLE := $(basename $(notdir $(wildcard config/*.mk)))

ifeq ($(BUILD_TYPE),)
BUILD_TYPE := debug
endif

ifeq ($(BUILD_TYPE),release)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -g -O0 -DDEBUG -DTRACE
endif

CFLAGS += -Wall -Iinclude

ifeq ($(TRANSPORT),)
ifneq ($(wildcard config/$(PLATFORM).mk),)
ERRMSG := config/$(PLATFORM).mk does not set TRANSPORT
else
ERRMSG := no configuration for platform '$(PLATFORM)'
endif
.PHONY: all
all:
	@echo "ERROR: $(ERRMSG)."
	@echo "       available: $(AVAILABLE)"
	@echo "       retry as:  make PLATFORM=<name>"
	@exit 1
else
.PHONY: all
all: $(EXE)

$(EXE): $(ALL_OBJS)
	$(CC) $(ALL_OBJS) $(LDFLAGS) $(LIBS) -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

transport.o: transport/transport.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TRANSPORT).o: transport/$(TRANSPORT).c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
endif

.PHONY: test
test: all
	./$(EXE) --help

.PHONY: clean
clean:
	rm -f $(EXE) $(ALL_OBJS)

.PHONY: info
info:
	@echo "platform:   $(PLATFORM) (uname: $(UNAME_S))"
	@echo "build type: $(BUILD_TYPE)"
	@echo "cc:         $(CC)"
	@echo "cflags:     $(CFLAGS)"
	@echo "ldflags:    $(LDFLAGS)"
	@echo "libs:       $(LIBS)"
	@echo "transport:  $(TRANSPORT)"

.PHONY: help
help:
	@echo "usage: make [target] [PLATFORM=<os>] [BUILD_TYPE=debug|release] [CC=<compiler>]"
	@echo "targets: all (default), test, clean, info, help"
	@echo "platforms: $(AVAILABLE)"
