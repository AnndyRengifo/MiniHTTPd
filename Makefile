CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE -Iinclude -g
SRC     = $(wildcard src/*.c)
OBJ     = $(patsubst src/%.c,build/%.o,$(SRC))
HDR     = $(wildcard include/*.h)
TARGET  = minihttpd

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c $(HDR) | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean