#!/bin/sh
set -eu

# Load redroid kernel modules and set up Android IPC devices.
# Run as root or with sudo.

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
ASHMEM_KO=${ASHMEM_KO:-$SCRIPT_DIR/ashmem/ashmem_linux.ko}
BINDER_KO=${BINDER_KO:-$SCRIPT_DIR/binder/binder_linux.ko}
BINDERFS_MNT=${BINDERFS_MNT:-/dev/binderfs}

load_mod() {
    name=$1
    ko=$2

    if lsmod | grep -Eq "^${name}[[:space:]]"; then
        printf '  %s already loaded\n' "$name"
        return 0
    fi

    if command -v modprobe >/dev/null 2>&1 && modprobe "$name" 2>/dev/null; then
        printf '  Loaded %s via modprobe\n' "$name"
        return 0
    fi

    if [ -f "$ko" ]; then
        printf '  modprobe failed; falling back to insmod %s\n' "$ko"
        insmod "$ko"
        return 0
    fi

    printf 'ERROR: cannot load %s (modprobe failed and %s not found)\n' "$name" "$ko" >&2
    return 1
}

binderfs_mounted() {
    awk -v target="$BINDERFS_MNT" '$2 == target && $3 == "binder" { found = 1 } END { exit(found ? 0 : 1) }' /proc/mounts
}

echo "Loading ashmem_linux..."
load_mod ashmem_linux "$ASHMEM_KO"

echo "Loading binder_linux..."
load_mod binder_linux "$BINDER_KO"

if ! binderfs_mounted; then
    echo "Mounting binderfs at $BINDERFS_MNT..."
    mkdir -p "$BINDERFS_MNT"
    mount -t binder binder "$BINDERFS_MNT"
fi

echo
echo "Module status:"
lsmod | grep -E "binder|ashmem" || true
echo
echo "Devices:"
[ -e /dev/ashmem ] && echo "  /dev/ashmem" || echo "  /dev/ashmem (not found)"
[ -d "$BINDERFS_MNT" ] && ls -la "$BINDERFS_MNT"/ 2>/dev/null | head -10 || true
echo
echo "Done."
