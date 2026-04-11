#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=""
PREBUILT_ROOT="$ROOT_DIR/bin/prebuilt"
OUTPUT_DIR="$ROOT_DIR/bin"
BUNDLE_NAME="redroid-modules-standalone"
KEEP_STAGE=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --version VERSION        Release version to embed in the bundle
  --prebuilt-root DIR      Root containing prebuilt/<kernel> module directories
  --output-dir DIR         Destination for the generated .run installer
  --bundle-name NAME       Base output name (default: $BUNDLE_NAME)
  --keep-stage             Keep the temporary staging directory for inspection
  --help                   Show this help text
EOF
}

copy_clean_tree() {
    src_dir=$1
    dst_dir=$2

    mkdir -p "$dst_dir"
    (
        cd "$src_dir"
        tar \
            --exclude-vcs \
            --exclude='*.o' \
            --exclude='*.ko' \
            --exclude='*.mod' \
            --exclude='*.mod.c' \
            --exclude='*.cmd' \
            --exclude='*.symvers' \
            --exclude='Module.symvers' \
            --exclude='modules.order' \
            --exclude='.tmp_versions' \
            --exclude='.gradle' \
            --exclude='build' \
            --exclude='app/build' \
            --exclude='local.properties' \
            --exclude='*.apk' \
            -cf - .
    ) | (
        cd "$dst_dir"
        tar -xf -
    )
}

render_run_stub() {
    output_file=$1

    cat > "$output_file" <<'EOF'
#!/bin/sh
set -eu
umask 022

TARGET_DIR=
EXTRACT_ONLY=0
ASSUME_YES=0
INTERACTIVE=1
SKIP_TEST=0
ALLOW_DKMS=1
PREFIX=/
LOG_FILE=${REDROID_INSTALL_LOG:-/var/log/redroid-modules-install.log}

usage() {
    cat <<'USAGE'
redroid-modules self-extracting installer

Usage:
  ./redroid-modules-standalone-<version>.run [options]

Options:
  --extract-only          Only unpack the bundle, do not install
  --target DIR            Extract to DIR instead of a temporary directory
  --yes, -y               Accept prompts automatically
  --silent, --non-interactive
                          Run without interactive prompts
  --no-dkms               Disable DKMS fallback when no prebuilt module exists
  --skip-test             Skip the post-install ipcverify-host validation
  --prefix DIR            Alternate installation prefix (advanced)
  --help, -h              Show this help text
USAGE
}

setup_logging() {
    if [ "${REDROID_LOGGING_INITIALIZED:-0}" = "1" ]; then
        return 0
    fi

    if ! mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null; then
        LOG_FILE="${TMPDIR:-/tmp}/redroid-modules-install.log"
    fi
    if ! : >> "$LOG_FILE" 2>/dev/null; then
        LOG_FILE="${TMPDIR:-/tmp}/redroid-modules-install.log"
        : > "$LOG_FILE"
    fi

    if command -v mkfifo >/dev/null 2>&1 && command -v tee >/dev/null 2>&1; then
        pipe_path="${TMPDIR:-/tmp}/redroid-selfextract.$$.$$.pipe"
        rm -f "$pipe_path"
        mkfifo "$pipe_path"
        tee -a "$LOG_FILE" < "$pipe_path" &
        tee_pid=$!
        exec > "$pipe_path" 2>&1
        rm -f "$pipe_path"
        export tee_pid pipe_path
    else
        exec >> "$LOG_FILE" 2>&1
    fi

    export REDROID_INSTALL_LOG="$LOG_FILE"
    export REDROID_LOGGING_INITIALIZED=1
    printf 'Logging to %s\n' "$LOG_FILE"
}

cleanup() {
    if [ "${AUTO_TARGET_CLEANUP:-0}" = "1" ] && [ -n "${TARGET_DIR:-}" ] && [ -d "$TARGET_DIR" ]; then
        rm -rf "$TARGET_DIR"
    fi
}

extract_payload() {
    archive_line=$(awk '/^__REDROID_ARCHIVE_BELOW__$/ { print NR + 1; exit 0; }' "$0")
    if [ -z "$archive_line" ]; then
        echo "ERROR: unable to locate embedded payload." >&2
        exit 1
    fi
    tail -n +"$archive_line" "$0" | tar -xz -C "$TARGET_DIR"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --extract-only)
            EXTRACT_ONLY=1
            shift
            ;;
        --target)
            TARGET_DIR=$2
            shift 2
            ;;
        --yes|-y)
            ASSUME_YES=1
            shift
            ;;
        --silent|--non-interactive)
            INTERACTIVE=0
            ASSUME_YES=1
            shift
            ;;
        --no-dkms)
            ALLOW_DKMS=0
            shift
            ;;
        --skip-test)
            SKIP_TEST=1
            shift
            ;;
        --prefix)
            PREFIX=$2
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

setup_logging

if [ -z "${TARGET_DIR:-}" ]; then
    TARGET_DIR=$(mktemp -d "${TMPDIR:-/tmp}/redroid-modules.XXXXXX")
    AUTO_TARGET_CLEANUP=1
else
    mkdir -p "$TARGET_DIR"
    AUTO_TARGET_CLEANUP=0
fi
trap cleanup EXIT INT TERM HUP

extract_payload

bundle_dir=$(find "$TARGET_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)
if [ -z "$bundle_dir" ]; then
    echo "ERROR: extracted bundle contents are missing." >&2
    exit 1
fi

printf 'Bundle extracted to %s\n' "$bundle_dir"

if [ "$EXTRACT_ONLY" -eq 1 ]; then
    echo "Extraction complete."
    AUTO_TARGET_CLEANUP=0
    exit 0
fi

set -- --bundle-dir "$bundle_dir"
[ "$ASSUME_YES" -eq 1 ] && set -- "$@" --yes
[ "$INTERACTIVE" -eq 0 ] && set -- "$@" --non-interactive
[ "$ALLOW_DKMS" -eq 0 ] && set -- "$@" --no-dkms
[ "$SKIP_TEST" -eq 1 ] && set -- "$@" --skip-test
[ "$PREFIX" != "/" ] && set -- "$@" --prefix "$PREFIX"

if sh "$bundle_dir/install.sh" "$@"; then
    exit 0
fi

AUTO_TARGET_CLEANUP=0
exit 1
__REDROID_ARCHIVE_BELOW__
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --version)
            VERSION=$2
            shift 2
            ;;
        --prebuilt-root)
            PREBUILT_ROOT=$2
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR=$2
            shift 2
            ;;
        --bundle-name)
            BUNDLE_NAME=$2
            shift 2
            ;;
        --keep-stage)
            KEEP_STAGE=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ -z "$VERSION" ]; then
    VERSION=$(sh "$ROOT_DIR/scripts/read-version.sh")
fi

if [ ! -d "$PREBUILT_ROOT" ]; then
    echo "ERROR: prebuilt root '$PREBUILT_ROOT' does not exist." >&2
    exit 1
fi

if [ ! -x "$ROOT_DIR/ipcverify/build/ipcverify-host" ]; then
    make -C "$ROOT_DIR/ipcverify" host OUTPUT_DIR="$ROOT_DIR/bin"
fi

mkdir -p "$OUTPUT_DIR"
STAGE_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/${BUNDLE_NAME}.XXXXXX")
BUNDLE_DIR="$STAGE_ROOT/${BUNDLE_NAME}-${VERSION}"
RUN_OUTPUT="$OUTPUT_DIR/${BUNDLE_NAME}-${VERSION}.run"
LAYOUT_OUTPUT="$OUTPUT_DIR/release-layout.txt"

mkdir -p "$BUNDLE_DIR/prebuilt" "$BUNDLE_DIR/scripts" "$BUNDLE_DIR/bin"

copy_clean_tree "$ROOT_DIR/binder" "$BUNDLE_DIR/binder"
copy_clean_tree "$ROOT_DIR/ashmem" "$BUNDLE_DIR/ashmem"
copy_clean_tree "$ROOT_DIR/scripts" "$BUNDLE_DIR/scripts"
copy_clean_tree "$ROOT_DIR/ipcverify" "$BUNDLE_DIR/ipcverify"

cp "$ROOT_DIR/Makefile" "$BUNDLE_DIR/Makefile"
cp "$ROOT_DIR/VERSION" "$BUNDLE_DIR/VERSION"
cp "$ROOT_DIR/load_modules.sh" "$BUNDLE_DIR/load_modules.sh"
cp "$ROOT_DIR/packaging/run-installer/install.sh" "$BUNDLE_DIR/install.sh"
cp "$ROOT_DIR/README.md" "$BUNDLE_DIR/README.md"

chmod +x "$BUNDLE_DIR/install.sh" "$BUNDLE_DIR/load_modules.sh"

find "$PREBUILT_ROOT" -mindepth 1 -maxdepth 1 -type d | while read -r kernel_dir; do
    kernel_name=$(basename "$kernel_dir")
    mkdir -p "$BUNDLE_DIR/prebuilt/$kernel_name"
    cp "$kernel_dir/binder_linux.ko" "$BUNDLE_DIR/prebuilt/$kernel_name/binder_linux.ko"
    cp "$kernel_dir/ashmem_linux.ko" "$BUNDLE_DIR/prebuilt/$kernel_name/ashmem_linux.ko"
    if [ -f "$kernel_dir/ipcverify-host" ]; then
        cp "$kernel_dir/ipcverify-host" "$BUNDLE_DIR/prebuilt/$kernel_name/ipcverify-host"
    fi
    if [ -f "$kernel_dir/ipcverify.apk" ]; then
        cp "$kernel_dir/ipcverify.apk" "$BUNDLE_DIR/prebuilt/$kernel_name/ipcverify.apk"
    fi
    if [ -f "$kernel_dir/KERNEL" ]; then
        cp "$kernel_dir/KERNEL" "$BUNDLE_DIR/prebuilt/$kernel_name/KERNEL"
    fi
    if [ -f "$kernel_dir/SHA256SUMS" ]; then
        cp "$kernel_dir/SHA256SUMS" "$BUNDLE_DIR/prebuilt/$kernel_name/SHA256SUMS"
    fi
    if [ -f "$kernel_dir/VERSION" ]; then
        cp "$kernel_dir/VERSION" "$BUNDLE_DIR/prebuilt/$kernel_name/VERSION"
    fi
done

cp "$ROOT_DIR/ipcverify/build/ipcverify-host" "$BUNDLE_DIR/bin/ipcverify-host"
chmod +x "$BUNDLE_DIR/bin/ipcverify-host"
if [ -f "$ROOT_DIR/bin/ipcverify.apk" ]; then
    cp "$ROOT_DIR/bin/ipcverify.apk" "$BUNDLE_DIR/bin/ipcverify.apk"
elif [ -f "$ROOT_DIR/ipcverify/app/build/outputs/apk/debug/ipcverify.apk" ]; then
    cp "$ROOT_DIR/ipcverify/app/build/outputs/apk/debug/ipcverify.apk" "$BUNDLE_DIR/bin/ipcverify.apk"
fi

sed -i -E "s/^(PACKAGE_VERSION=\").*(\")/\1${VERSION}\2/" "$BUNDLE_DIR/binder/dkms.conf" "$BUNDLE_DIR/ashmem/dkms.conf"

cat > "$BUNDLE_DIR/RELEASE_MANIFEST.txt" <<EOF
name=${BUNDLE_NAME}
version=${VERSION}
generated_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)
prebuilt_root=${PREBUILT_ROOT}
EOF

(
    cd "$STAGE_ROOT"
    tar -czf "${BUNDLE_NAME}-${VERSION}.tar.gz" "${BUNDLE_NAME}-${VERSION}"
)

render_run_stub "$RUN_OUTPUT"
cat "$STAGE_ROOT/${BUNDLE_NAME}-${VERSION}.tar.gz" >> "$RUN_OUTPUT"
chmod +x "$RUN_OUTPUT"

{
    echo "${BUNDLE_NAME}-${VERSION}/"
    find "$BUNDLE_DIR" -mindepth 1 -printf '%P\n' | sed 's#^#  #'
} | sort > "$LAYOUT_OUTPUT"

printf 'Created %s\n' "$RUN_OUTPUT"
printf 'Wrote bundle layout to %s\n' "$LAYOUT_OUTPUT"

if [ "$KEEP_STAGE" -eq 1 ]; then
    printf 'Kept staging directory at %s\n' "$STAGE_ROOT"
else
    rm -rf "$STAGE_ROOT"
fi
