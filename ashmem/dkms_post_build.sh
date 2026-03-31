#!/bin/sh
# DKMS post-build hook for ashmem_linux.
# Validates that the built module has the expected vermagic.
set -e

KVER="${kernelver:-$(uname -r)}"

echo "redroid-ashmem: post-build verification for kernel $KVER"

if [ -f "ashmem_linux.ko" ]; then
    VERMAGIC=$(modinfo -F vermagic ashmem_linux.ko 2>/dev/null || echo "unknown")
    echo "  vermagic: $VERMAGIC"
    if echo "$VERMAGIC" | grep -qF "$KVER"; then
        echo "  OK: vermagic matches target kernel"
    else
        echo "  WARNING: vermagic does not match $KVER" >&2
    fi
else
    echo "  WARNING: ashmem_linux.ko not found after build" >&2
fi
