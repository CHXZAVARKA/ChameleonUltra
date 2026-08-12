CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -fshort-enums

SOURCE_DIR := ../src
SLOT_TEST_BINARY := build/test_slot_reorder
LED_TEST_BINARY := build/test_boot_rainbow
BATTERY_TEST_BINARY := build/test_battery_indicator

.PHONY: test clean

test: $(SLOT_TEST_BINARY) $(LED_TEST_BINARY) $(BATTERY_TEST_BINARY)
	$(SLOT_TEST_BINARY)
	$(LED_TEST_BINARY)
	$(BATTERY_TEST_BINARY)
	python3 test_app_cmd_contract.py

$(SLOT_TEST_BINARY): test_slot_reorder.c \
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

$(LED_TEST_BINARY): test_boot_rainbow.c $(SOURCE_DIR)/rgb/rainbow_color.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SOURCE_DIR)/rgb $^ -o $@

$(BATTERY_TEST_BINARY): test_battery_indicator.c $(SOURCE_DIR)/rgb/battery_indicator.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SOURCE_DIR)/rgb $^ -o $@

clean:
	rm -rf build
