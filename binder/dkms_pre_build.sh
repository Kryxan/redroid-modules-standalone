#!/bin/sh
# DKMS pre-build hook for binder_linux.
# Validates that kernel headers are usable before attempting the build.
set -e

KVER="${kernelver:-$(uname -r)}"
KDIR="/lib/modules/$KVER/build"

echo "redroid-binder: pre-build check for kernel $KVER"

if [ ! -d "$KDIR" ]; then
    echo "ERROR: kernel headers not found at $KDIR" >&2
    exit 1
fi

if [ ! -f "$KDIR/Makefile" ]; then
    echo "ERROR: $KDIR/Makefile missing — headers may be incomplete" >&2
    exit 1
fi

# Verify key headers exist that our compat layer depends on
for hdr in linux/version.h linux/fs.h linux/mm.h linux/security.h; do
    if [ ! -f "$KDIR/include/$hdr" ]; then
        echo "WARNING: $KDIR/include/$hdr not found" >&2
    fi
done

echo "redroid-binder: pre-build checks passed"
