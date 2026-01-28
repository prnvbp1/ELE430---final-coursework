CC      := gcc
TARGET  := learn_cw
SRC_DIR := source
INC_DIR := header
BUILD   := build

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

# ---- Common flags ----
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow
INCLUDE := -I$(INC_DIR)
PTHREAD := -pthread

# Optional: ensure POSIX APIs available on Linux Mint (usually not required, but harmless)
DEFS := -D_POSIX_C_SOURCE=200809L

CFLAGS_RELEASE := $(CSTD) $(WARN) -O2 -DNDEBUG $(INCLUDE) $(DEFS)
CFLAGS_DEBUG   := $(CSTD) $(WARN) -O0 -g3 -DDEBUG $(INCLUDE) $(DEFS)

LDFLAGS := $(PTHREAD)
LDLIBS  :=

.PHONY: all release debug clean run run_debug info

all: release

release: CFLAGS := $(CFLAGS_RELEASE)
release: $(BUILD)/$(TARGET)

debug: CFLAGS := $(CFLAGS_DEBUG)
debug: $(BUILD)/$(TARGET)_debug

$(BUILD)/$(TARGET): $(OBJS) | $(BUILD)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/$(TARGET)_debug: $(OBJS) | $(BUILD)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(PTHREAD) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

ARGS ?= -p 5 -c 3 -q 10 -t 10

run: release
	./$(BUILD)/$(TARGET) $(ARGS)

run_debug: debug
	./$(BUILD)/$(TARGET)_debug $(ARGS) -v

info:
	@echo "SRCS  = $(SRCS)"
	@echo "OBJS  = $(OBJS)"
	@echo "TARGET= $(TARGET)"
	@echo "ARGS  = $(ARGS)"

clean:
	rm -rf $(BUILD) *.o *.exe run_log.csv evidence sweep results
