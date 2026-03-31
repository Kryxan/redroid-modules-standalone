#!/bin/bash
set -e

# Load redroid kernel modules and set up Android IPC devices.
# Run as root or with sudo.

# Prefer modprobe (requires 'make install' first) with insmod fallback.
load_mod() {
    local name="$1"
    local ko="$2"
    if lsmod | grep -q "^${name} "; then
        echo "  $name already loaded"
        return 0
    fi
    if modprobe "$name" 2>/dev/null; then
        echo "  Loaded $name via modprobe"
    elif [ -f "$ko" ]; then
        echo "  modprobe failed; falling back to insmod $ko"
        insmod "$ko"
    else
        echo "ERROR: cannot load $name (modprobe failed and $ko not found)" >&2
        exit 1
    fi
}

echo "Loading ashmem_linux..."
load_mod ashmem_linux ashmem/ashmem_linux.ko

echo "Loading binder_linux..."
load_mod binder_linux binder/binder_linux.ko

# Mount binderfs if not already mounted.
BINDERFS_MNT="/dev/binderfs"
if ! mountpoint -q "$BINDERFS_MNT" 2>/dev/null; then
    echo "Mounting binderfs at $BINDERFS_MNT..."
    mkdir -p "$BINDERFS_MNT"
    mount -t binder binder "$BINDERFS_MNT"
fi

echo ""
echo "Module status:"
lsmod | grep -E "binder|ashmem" || true
echo ""
echo "Devices:"
[ -e /dev/ashmem ] && echo "  /dev/ashmem" || echo "  /dev/ashmem (not found)"
[ -d "$BINDERFS_MNT" ] && ls -la "$BINDERFS_MNT"/ 2>/dev/null | head -10 || true
echo ""
echo "Done."
