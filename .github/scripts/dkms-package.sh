#!/bin/sh
# dkms-package.sh — Build DKMS .deb packages for both modules.
#
# Usage: dkms-package.sh [VERSION]
#
# Produces:
#   dist/redroid-binder-dkms_VERSION_all.deb
#   dist/redroid-ashmem-dkms_VERSION_all.deb
set -eu

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
SRCROOT=$(CDPATH='' && cd -- "$SCRIPT_DIR/../.." && pwd)
VERSION=${1:-$(tr -d ' \t\r\n' < "$SRCROOT/VERSION")}
DIST_DIR="$SRCROOT/dist"

if ! printf '%s\n' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: invalid semantic version '$VERSION'" >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

build_dkms_deb() {
    module_dir=$1
    pkg_name=$2
    built_module=$3
    stage_dir="/tmp/dkms-stage-${pkg_name}"
    src_dest="/usr/src/${pkg_name}-${VERSION}"
    deb_name="${pkg_name}-dkms_${VERSION}_all.deb"

    echo "=== Building $pkg_name $VERSION ==="

    rm -rf "$stage_dir"
    mkdir -p "$stage_dir/$src_dest" "$stage_dir/DEBIAN"

    cp -a "$SRCROOT/$module_dir"/*.c "$stage_dir/$src_dest/" 2>/dev/null || true
    cp -a "$SRCROOT/$module_dir"/*.h "$stage_dir/$src_dest/" 2>/dev/null || true
    cp -a "$SRCROOT/$module_dir"/Makefile "$stage_dir/$src_dest/"
    cp -a "$SRCROOT/$module_dir"/Kconfig "$stage_dir/$src_dest/" 2>/dev/null || true

    for subdir in uapi linux patches ion; do
        if [ -d "$SRCROOT/$module_dir/$subdir" ]; then
            cp -a "$SRCROOT/$module_dir/$subdir" "$stage_dir/$src_dest/"
        fi
    done

    cat > "$stage_dir/$src_dest/dkms.conf" <<DKMSEOF
PACKAGE_NAME="$pkg_name"
PACKAGE_VERSION="$VERSION"
CLEAN="make clean"
MAKE[0]="make all KERNEL_SRC=/lib/modules/\$kernelver/build"
BUILT_MODULE_NAME[0]="$built_module"
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="yes"
REMAKE_INITRD="no"
DKMSEOF

    cat > "$stage_dir/DEBIAN/control" <<CTLEOF
Package: ${pkg_name}-dkms
Version: ${VERSION}
Architecture: all
Depends: dkms (>= 2.1), build-essential
Maintainer: redroid-modules maintainers
Description: DKMS package for ${built_module} kernel module
 Out-of-tree ${built_module} kernel module for redroid (Android-in-Linux).
 Uses DKMS to automatically rebuild against installed kernel headers.
CTLEOF

    cat > "$stage_dir/DEBIAN/postinst" <<'POSTEOF'
#!/bin/sh
set -e
PACKAGE_NAME="__PKG__"
PACKAGE_VERSION="__VER__"

if [ "$1" = "configure" ] && command -v dkms >/dev/null 2>&1; then
    dkms add -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" 2>/dev/null || true
    dkms build -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" || true
    dkms install -m "$PACKAGE_NAME" -v "$PACKAGE_VERSION" --force || true
fi
POSTEOF
    sed -i "s/__PKG__/$pkg_name/g; s/__VER__/$VERSION/g" "$stage_dir/DEBIAN/postinst"
    chmod 755 "$stage_dir/DEBIAN/postinst"

    cat > "$stage_dir/DEBIAN/prerm" <<'RMEOF'
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
    sed -i "s/__PKG__/$pkg_name/g; s/__VER__/$VERSION/g" "$stage_dir/DEBIAN/prerm"
    chmod 755 "$stage_dir/DEBIAN/prerm"

    dpkg-deb --build "$stage_dir" "$DIST_DIR/$deb_name"
    echo "  -> $DIST_DIR/$deb_name"

    rm -rf "$stage_dir"
}

build_dkms_deb "ashmem" "redroid-ashmem" "ashmem_linux"
build_dkms_deb "binder" "redroid-binder" "binder_linux"

echo
echo "=== Packages ==="
ls -lh "$DIST_DIR"/*.deb
