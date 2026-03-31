#!/usr/bin/env bash
# dkms-package.sh — Build DKMS .deb packages for both modules.
#
# Usage: dkms-package.sh [VERSION]
#
# Produces:
#   dist/redroid-binder-dkms_VERSION_all.deb
#   dist/redroid-ashmem-dkms_VERSION_all.deb
set -euo pipefail

VERSION="${1:-2.0.0}"
DIST_DIR="dist"
SRCROOT="$(cd "$(dirname "$0")/../.." && pwd)"

mkdir -p "$DIST_DIR"

build_dkms_deb() {
    local module_dir="$1"    # ashmem or binder
    local pkg_name="$2"      # redroid-ashmem or redroid-binder
    local built_module="$3"  # ashmem_linux or binder_linux

    echo "=== Building $pkg_name $VERSION ==="

    local STAGE="/tmp/dkms-stage-${pkg_name}"
    local SRC_DEST="/usr/src/${pkg_name}-${VERSION}"
    rm -rf "$STAGE"
    mkdir -p "$STAGE/$SRC_DEST"
    mkdir -p "$STAGE/DEBIAN"

    # Copy module source
    cp -a "$SRCROOT/$module_dir"/*.c "$STAGE/$SRC_DEST/" 2>/dev/null || true
    cp -a "$SRCROOT/$module_dir"/*.h "$STAGE/$SRC_DEST/" 2>/dev/null || true
    cp -a "$SRCROOT/$module_dir"/Makefile "$STAGE/$SRC_DEST/"
    cp -a "$SRCROOT/$module_dir"/Kconfig "$STAGE/$SRC_DEST/" 2>/dev/null || true

    # Copy subdirectories (uapi/, linux/, patches/)
    for subdir in uapi linux patches; do
        if [ -d "$SRCROOT/$module_dir/$subdir" ]; then
            cp -a "$SRCROOT/$module_dir/$subdir" "$STAGE/$SRC_DEST/"
        fi
    done

    # Generate dkms.conf with correct version
    cat > "$STAGE/$SRC_DEST/dkms.conf" <<DKMSEOF
PACKAGE_NAME="$pkg_name"
PACKAGE_VERSION="$VERSION"
CLEAN="make clean"
MAKE[0]="make all KERNEL_SRC=/lib/modules/\$kernelver/build"
BUILT_MODULE_NAME[0]="$built_module"
DEST_MODULE_LOCATION[0]="/extra"
AUTOINSTALL="yes"
REMAKE_INITRD="no"
DKMSEOF

    # DEBIAN control file
    cat > "$STAGE/DEBIAN/control" <<CTLEOF
Package: ${pkg_name}-dkms
Version: ${VERSION}
Architecture: all
Depends: dkms (>= 2.1), build-essential
Maintainer: redroid-modules maintainers
Description: DKMS package for ${built_module} kernel module
 Out-of-tree ${built_module} kernel module for redroid (Android-in-Linux).
 Uses DKMS to automatically rebuild against installed kernel headers.
CTLEOF

    # postinst: register and build with DKMS
    cat > "$STAGE/DEBIAN/postinst" <<'POSTEOF'
#!/bin/sh
set -e
PACKAGE_NAME="__PKG__"
PACKAGE_VERSION="__VER__"

if [ "$1" = "configure" ]; then
    if command -v dkms >/dev/null 2>&1; then
        dkms add -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" 2>/dev/null || true
        dkms build -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" || true
        dkms install -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" --force || true
    fi
fi
POSTEOF
    sed -i "s/__PKG__/$pkg_name/g; s/__VER__/$VERSION/g" "$STAGE/DEBIAN/postinst"
    chmod 755 "$STAGE/DEBIAN/postinst"

    # prerm: unregister from DKMS
    cat > "$STAGE/DEBIAN/prerm" <<'RMEOF'
#!/bin/sh
set -e
PACKAGE_NAME="__PKG__"
PACKAGE_VERSION="__VER__"

if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    if command -v dkms >/dev/null 2>&1; then
        dkms remove -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" --all 2>/dev/null || true
    fi
fi
RMEOF
    sed -i "s/__PKG__/$pkg_name/g; s/__VER__/$VERSION/g" "$STAGE/DEBIAN/prerm"
    chmod 755 "$STAGE/DEBIAN/prerm"

    # Build the .deb
    local deb_name="${pkg_name}-dkms_${VERSION}_all.deb"
    dpkg-deb --build "$STAGE" "$DIST_DIR/$deb_name"
    echo "  -> $DIST_DIR/$deb_name"

    rm -rf "$STAGE"
}

build_dkms_deb "ashmem" "redroid-ashmem" "ashmem_linux"
build_dkms_deb "binder" "redroid-binder" "binder_linux"

echo ""
echo "=== Packages ==="
ls -lh "$DIST_DIR"/*.deb
