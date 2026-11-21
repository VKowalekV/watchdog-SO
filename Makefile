CC ?= gcc
CFLAGS = -Wall -Wextra -Iinclude $(EXTRA_CFLAGS)
TARGET=build/watchdog
SRC=watchdog.c src/config.c src/daemon.c src/logger.c src/process.c 

all: $(TARGET) build/tests/RAM_test build/tests/CPU_test

$(TARGET): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

build/tests/RAM_test: tests/RAM_test.c
	mkdir -p build/tests
	$(CC) $(CFLAGS) tests/RAM_test.c -o build/tests/RAM_test

build/tests/CPU_test: tests/CPU_test.c
	mkdir -p build/tests
	$(CC) $(CFLAGS) tests/CPU_test.c -o build/tests/CPU_test

clean:
	rm -rf build

.PHONY: all clean
