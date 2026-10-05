CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Iinclude
LDLIBS  := -pthread -lrt -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: disksim

disksim: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c include/disksim.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: disksim
	./tests/run_tests.sh

clean:
	rm -rf build disksim

.PHONY: all test clean
