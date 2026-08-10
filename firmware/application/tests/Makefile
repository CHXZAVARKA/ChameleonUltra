CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -fshort-enums

SOURCE_DIR := ../src
TEST_BINARY := build/test_slot_reorder
RAINBOW_TEST_BINARY := build/test_rainbow_color

.PHONY: test clean

test: $(TEST_BINARY) $(RAINBOW_TEST_BINARY)
	$(TEST_BINARY)
	$(RAINBOW_TEST_BINARY)
	python3 test_app_cmd_contract.py
	python3 test_led_animation_contract.py

$(TEST_BINARY): test_slot_reorder.c \
	$(SOURCE_DIR)/rfid/nfctag/slot_reorder.c \
	$(SOURCE_DIR)/rfid/nfctag/slot_reorder_protocol.c \
	$(SOURCE_DIR)/rfid/nfctag/tag_persistence.c
	mkdir -p build
	$(CC) $(CFLAGS) \
		-Iinclude \
		-I$(SOURCE_DIR) \
		-I$(SOURCE_DIR)/utils \
		-I$(SOURCE_DIR)/rfid/nfctag \
		$^ -o $@

$(RAINBOW_TEST_BINARY): test_rainbow_color.c \
	$(SOURCE_DIR)/rgb/rainbow_color.c
	mkdir -p build
	$(CC) $(CFLAGS) \
		-I$(SOURCE_DIR)/rgb \
		$^ -o $@

clean:
	rm -rf build
