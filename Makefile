CROSS ?= x86_64-elf
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
MTC ?= mtc

APPS_DIR ?= ../apps
OUT_DIR ?= build
LD_SCRIPT ?= $(APPS_DIR)/linker.ld
UAPI_DIR ?= ../phobos-kernel/uapi

CFLAGS := -ffreestanding -mno-red-zone -fno-pic -mcmodel=large -fno-builtin \
	-I $(UAPI_DIR) -I . -I rendering

BIN := $(OUT_DIR)/deimos
INPUT_BRIDGE_SRC := $(wildcard window_manager/input_bridge.c)
INPUT_BRIDGE_OBJ := $(patsubst %.c,$(OUT_DIR)/%.o,$(INPUT_BRIDGE_SRC))
C_OBJS := \
	$(OUT_DIR)/config.o \
	$(OUT_DIR)/main.o \
	$(OUT_DIR)/mt_runtime.o \
	$(OUT_DIR)/rendering/rendering.o \
	$(OUT_DIR)/window_manager/state.o \
	$(INPUT_BRIDGE_OBJ)

MTC_OBJS := \
	$(OUT_DIR)/deimos_compositor_mtc.o \
	$(OUT_DIR)/deimos_window_manager_mtc.o
MTC_LINK_OBJS := \
	$(OUT_DIR)/deimos_compositor_mtc.o

.PHONY: all mtc stage clean

all: $(BIN)

$(BIN): $(C_OBJS) $(MTC_LINK_OBJS) $(LD_SCRIPT)
	$(LD) -T $(LD_SCRIPT) -o $@ $(C_OBJS) $(MTC_LINK_OBJS)

$(OUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/deimos_compositor_mtc.o: compositor/compositor.mtc
	@mkdir -p $(dir $@)
	$(MTC) --no-runtime --no-libc --opt-level 2 -o $< $@

$(OUT_DIR)/deimos_window_manager_mtc.o: window_manager/window_manager.mtc
	@mkdir -p $(dir $@)
	$(MTC) --no-runtime --no-libc --opt-level 2 -o $< $@

mtc: $(MTC_OBJS)

stage: $(BIN)
	@mkdir -p $(APPS_DIR)/deimos
	cp $(BIN) $(APPS_DIR)/deimos/deimos

clean:
	rm -rf $(OUT_DIR)
