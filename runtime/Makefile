# C runtime linked into every Astra program → $(BUILD)/libastra_rt.a

CC    ?= cc
AR    ?= ar
BUILD ?= ../build

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude

SRCS := $(shell find src -name '*.c' | sort)
OBJS := $(patsubst src/%.c,$(BUILD)/runtime/%.o,$(SRCS))

.PHONY: all clean

ifeq ($(SRCS),)
all:
	@echo "runtime: no sources yet (roadmap task 1.17)"
else
all: $(BUILD)/libastra_rt.a
endif

$(BUILD)/libastra_rt.a: $(OBJS)
	$(AR) rcs $@ $^

$(BUILD)/runtime/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD)/runtime $(BUILD)/libastra_rt.a
