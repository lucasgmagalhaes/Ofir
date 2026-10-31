# Stage 0 compiler (C11).
#   make            debug build with AddressSanitizer and UBSan
#   make release    optimized build
#   make test       build and run the C unit tests in tests/

CC       ?= cc
BUILD    ?= ../build/stage0
MODE     ?= debug
SANITIZE ?= address,undefined   # empty to disable (e.g. clang without compiler-rt)

WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wshadow -Wstrict-prototypes
CFLAGS   := -std=c11 -D_POSIX_C_SOURCE=200809L $(WARNINGS) -Isrc -Itests

ifeq ($(MODE),release)
CFLAGS  += -O2 -DNDEBUG
LDFLAGS :=
else
CFLAGS  += -O0 -g -fno-omit-frame-pointer
LDFLAGS :=
ifneq ($(strip $(SANITIZE)),)
CFLAGS  += -fsanitize=$(strip $(SANITIZE))
LDFLAGS += -fsanitize=$(strip $(SANITIZE))
endif
endif

SRCS      := $(shell find src -name '*.c' | sort)
LIB_SRCS  := $(filter-out src/main.c,$(SRCS))
OBJS      := $(patsubst src/%.c,$(BUILD)/obj/$(MODE)/%.o,$(SRCS))
LIB_OBJS  := $(patsubst src/%.c,$(BUILD)/obj/$(MODE)/%.o,$(LIB_SRCS))
TESTS     := $(patsubst tests/%.c,$(BUILD)/tests/%,$(wildcard tests/*.c))

.PHONY: all release test clean

all: $(BUILD)/astra

release:
	$(MAKE) MODE=release

$(BUILD)/astra: $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/obj/$(MODE)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/tests/%: tests/%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test: $(TESTS)
	@for t in $(TESTS); do echo "unit: $$t"; $$t || exit 1; done
	@echo "unit: all $(words $(TESTS)) test programs passed"

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
