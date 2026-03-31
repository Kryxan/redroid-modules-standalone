# Userspace test programs for redroid-modules
#
#   make              — build all tests
#   make run-ashmem   — run ashmem unit tests
#   make run-binder   — show binder_test usage
#   make run-ipc      — run combined IPC validation (requires root)
#   make run-all      — run ashmem + IPC tests (requires root)

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

BINDER_INCLUDES := -I../binder
ASHMEM_INCLUDES := -I../ashmem

TARGETS := ashmem_test binder_test test_ipc

all: $(TARGETS)

ashmem_test: ashmem_test.c
	$(CC) $(CFLAGS) $(ASHMEM_INCLUDES) -o $@ $<

binder_test: test.c
	$(CC) $(CFLAGS) $(BINDER_INCLUDES) -o $@ $<

test_ipc: test_ipc.c
	$(CC) $(CFLAGS) $(BINDER_INCLUDES) $(ASHMEM_INCLUDES) -o $@ $<

run-ashmem: ashmem_test
	@echo "=== Running ashmem test ==="
	@./ashmem_test

run-binder: binder_test
	@echo "=== Running binderfs test ==="
	@echo "Usage: ./binder_test <binder-control-path> <device-name>"
	@echo "Example: ./binder_test /dev/binderfs/binder-control mydevice"

run-ipc: test_ipc
	@echo "=== Running IPC validation (requires root) ==="
	@./test_ipc

run-all: ashmem_test test_ipc
	@echo "=== Running ashmem test ==="
	@./ashmem_test
	@echo ""
	@echo "=== Running IPC validation (requires root) ==="
	@./test_ipc

clean:
	rm -f $(TARGETS)

.PHONY: all run-ashmem run-binder run-ipc run-all clean
