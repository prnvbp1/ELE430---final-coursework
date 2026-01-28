# =========================
# ELE430 Producer-Consumer
# Makefile (Linux Mint / WSL)
# =========================
#
# Common usage:
#   make release
#   make debug
#   make run ARGS="-p 5 -c 3 -q 10 -t 10 -v"
#   make clean
#
# Override binary name:
#   make release TARGET=learn_cw

CC      ?= gcc
TARGET  ?= learn_cw
SRC_DIR := source_and_header
BUILD   := build

SRCS := $(wildcard $(SRC_DIR)/*.c)

OBJDIR_RELEASE := $(BUILD)/release
OBJDIR_DEBUG   := $(BUILD)/debug

OBJS_RELEASE := $(patsubst $(SRC_DIR)/%.c,$(OBJDIR_RELEASE)/%.o,$(SRCS))
DEPS_RELEASE := $(OBJS_RELEASE:.o=.d)
BIN_RELEASE  := $(OBJDIR_RELEASE)/$(TARGET)

OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJDIR_DEBUG)/%.o,$(SRCS))
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)
BIN_DEBUG  := $(OBJDIR_DEBUG)/$(TARGET)

# ---- Flags ----
CPPFLAGS += -I$(SRC_DIR) -D_POSIX_C_SOURCE=200809L

CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow
PTHREAD := -pthread

CFLAGS_COMMON  := $(CSTD) $(WARN) $(PTHREAD)
CFLAGS_RELEASE := $(CFLAGS_COMMON) -O2 -DNDEBUG
CFLAGS_DEBUG   := $(CFLAGS_COMMON) -O0 -g3 -DDEBUG

# Embed build type into the binary (used for run metadata/logging)
CPPFLAGS_RELEASE := -DBUILD_TYPE=\"release\"
CPPFLAGS_DEBUG   := -DBUILD_TYPE=\"debug\"

LDFLAGS ?=
LDLIBS  ?=

# If your toolchain needs atomic support, uncomment:
# LDLIBS += -latomic

.PHONY: all release debug clean run run_debug info

all: release

release: $(BIN_RELEASE)

debug: $(BIN_DEBUG)

$(BIN_RELEASE): $(OBJS_RELEASE)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(PTHREAD)

$(OBJDIR_RELEASE)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS_RELEASE) -MMD -MP -c $< -o $@

$(BIN_DEBUG): $(OBJS_DEBUG)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(PTHREAD)

$(OBJDIR_DEBUG)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS_DEBUG) -MMD -MP -c $< -o $@

# ---- Convenience run targets ----
# Override at command line, e.g.:
# make run ARGS="-p 5 -c 3 -q 10 -t 10 -v"
ARGS ?= -p 5 -c 3 -q 10 -t 10

run: release
	./$(BIN_RELEASE) $(ARGS)

run_debug: debug
	./$(BIN_DEBUG) $(ARGS) -v

info:
	@echo "SRCS       = $(SRCS)"
	@echo "BIN_RELEASE = $(BIN_RELEASE)"
	@echo "BIN_DEBUG   = $(BIN_DEBUG)"
	@echo "ARGS        = $(ARGS)"

clean:
	rm -rf $(BUILD) run_log.csv

-include $(DEPS_RELEASE) $(DEPS_DEBUG)

