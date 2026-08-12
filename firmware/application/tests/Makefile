CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic

SOURCE_DIR := ../src
TEST_BINARY := build/test_boot_rainbow

.PHONY: test clean

test: $(TEST_BINARY)
	$(TEST_BINARY)

$(TEST_BINARY): test_boot_rainbow.c $(SOURCE_DIR)/rgb/rainbow_color.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SOURCE_DIR)/rgb $^ -o $@

clean:
	rm -rf build
