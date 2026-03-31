#!/bin/sh
# DKMS post-build hook for binder_linux.
# Validates that the built module has the expected vermagic.
set -e

KVER="${kernelver:-$(uname -r)}"

echo "redroid-binder: post-build verification for kernel $KVER"

if [ -f "binder_linux.ko" ]; then
    VERMAGIC=$(modinfo -F vermagic binder_linux.ko 2>/dev/null || echo "unknown")
    echo "  vermagic: $VERMAGIC"
    if echo "$VERMAGIC" | grep -qF "$KVER"; then
        echo "  OK: vermagic matches target kernel"
    else
        echo "  WARNING: vermagic does not match $KVER" >&2
    fi
else
    echo "  WARNING: binder_linux.ko not found after build" >&2
fi
