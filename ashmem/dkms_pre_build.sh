#!/bin/sh
# DKMS pre-build hook for ashmem_linux.
# Validates that kernel headers are usable before attempting the build.
set -e

KVER="${kernelver:-$(uname -r)}"
KDIR="/lib/modules/$KVER/build"

echo "redroid-ashmem: pre-build check for kernel $KVER"

if [ ! -d "$KDIR" ]; then
    echo "ERROR: kernel headers not found at $KDIR" >&2
    exit 1
fi

if [ ! -f "$KDIR/Makefile" ]; then
    echo "ERROR: $KDIR/Makefile missing — headers may be incomplete" >&2
    exit 1
fi

echo "redroid-ashmem: pre-build checks passed"
