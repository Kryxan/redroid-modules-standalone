CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2

SRC_DIR := src
BUILD_DIR := build
OUTPUT_DIR ?= $(CURDIR)/../bin
HOST_BINARY := $(BUILD_DIR)/ipcverify-host
HOST_OUTPUT := $(OUTPUT_DIR)/ipcverify-host
HOST_SOURCES := $(SRC_DIR)/host_main.c $(SRC_DIR)/ipc_checks.c
HOST_INCLUDES := -I$(SRC_DIR) -I../binder -I../ashmem
APK_PATH := app/build/outputs/apk/debug/ipcverify.apk
APK_OUTPUT := $(OUTPUT_DIR)/ipcverify.apk

all: host android

host: $(HOST_BINARY)
	@mkdir -p "$(OUTPUT_DIR)"
	@cp -f "$(HOST_BINARY)" "$(HOST_OUTPUT)"

$(HOST_BINARY): $(HOST_SOURCES) $(SRC_DIR)/ipc_checks.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(HOST_INCLUDES) -o $@ $(HOST_SOURCES)

android:
	@chmod +x ./build-android.sh
	OUTPUT_DIR="$(OUTPUT_DIR)" ./build-android.sh

run: host
	./$(HOST_BINARY)

adb-run: host
	./$(HOST_BINARY) --verify-android

clean:
	rm -rf $(BUILD_DIR) .gradle .gradle-dist app/build local.properties

print-apk:
	@printf '%s\n' "$(APK_OUTPUT)"

.PHONY: all host android run adb-run clean print-apk
