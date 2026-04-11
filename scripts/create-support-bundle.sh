#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH='' && cd -- "$SCRIPT_DIR/.." && pwd)
OUTPUT_DIR=${1:-$ROOT_DIR/dist}
TIMESTAMP=$(date -u +%Y%m%dT%H%M%SZ)
BUNDLE_NAME="ipcverify-support-$TIMESTAMP"
STAGE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ipcverify-support.XXXXXX")
BUNDLE_DIR="$STAGE_DIR/$BUNDLE_NAME"
BUNDLE_PATH="$OUTPUT_DIR/$BUNDLE_NAME.tar.gz"

cleanup() {
    rm -rf "$STAGE_DIR"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$OUTPUT_DIR" "$BUNDLE_DIR/system" "$BUNDLE_DIR/logs" "$BUNDLE_DIR/repo"

{
    echo "timestamp=$TIMESTAMP"
    echo "hostname=$(hostname 2>/dev/null || echo unknown)"
    echo "cwd=$(pwd)"
} > "$BUNDLE_DIR/system/context.txt"

uname -a > "$BUNDLE_DIR/system/uname.txt" 2>/dev/null || true
[ -f /etc/os-release ] && cp /etc/os-release "$BUNDLE_DIR/system/os-release.txt"
lsmod | grep -E 'binder|ashmem' > "$BUNDLE_DIR/system/lsmod.txt" 2>/dev/null || true
modinfo binder_linux > "$BUNDLE_DIR/system/modinfo-binder.txt" 2>/dev/null || true
modinfo ashmem_linux > "$BUNDLE_DIR/system/modinfo-ashmem.txt" 2>/dev/null || true
dmesg | tail -n 200 > "$BUNDLE_DIR/system/dmesg-tail.txt" 2>/dev/null || true

if [ -x "$ROOT_DIR/scripts/detect-ipc-runtime.sh" ]; then
    bash "$ROOT_DIR/scripts/detect-ipc-runtime.sh" --format keyvalue > "$BUNDLE_DIR/system/detect-ipc-runtime.txt" 2>/dev/null || true
fi

if command -v git >/dev/null 2>&1; then
    (
        cd "$ROOT_DIR"
        git status --short > "$BUNDLE_DIR/repo/git-status.txt" 2>/dev/null || true
        git rev-parse HEAD > "$BUNDLE_DIR/repo/git-head.txt" 2>/dev/null || true
    )
fi

for log_root in "$ROOT_DIR/bin" "$ROOT_DIR/dist"; do
    if [ -d "$log_root" ]; then
        find "$log_root" -maxdepth 1 -type f \( -name '*.log' -o -name 'adb-ipcverify-*.txt' -o -name 'adb-ipcverify-logcat.txt' \) -exec cp {} "$BUNDLE_DIR/logs/" \; 2>/dev/null || true
    fi
done

(
    cd "$STAGE_DIR"
    tar -czf "$BUNDLE_PATH" "$BUNDLE_NAME"
)

printf '%s\n' "$BUNDLE_PATH"
