#!/bin/sh
set -eu
umask 022

BUNDLE_DIR=
PREFIX=/
ASSUME_YES=0
INTERACTIVE=1
ALLOW_DKMS=1
SKIP_TEST=0

KERNEL_RELEASE=
BUNDLE_VERSION=unknown
INSTALL_MODE=not-started
MODULE_STATUS=not-started
BINDERFS_STATUS=not-started
IPCVERIFY_STATUS=not-run
VERIFY_STATUS=not-run
SUMMARY_RESULT=SUCCESS
LOG_FILE=${REDROID_INSTALL_LOG:-/var/log/redroid-modules-install.log}

usage() {
    cat <<'EOF'
Usage: install.sh [options]

Options:
  --bundle-dir DIR      Use an already extracted release bundle
  --yes, -y             Accept prompts automatically
  --non-interactive     Do not prompt for confirmation
  --no-dkms             Do not fall back to DKMS when no prebuilt module exists
  --skip-test           Skip running installed ipcverify-host after installation
  --prefix DIR          Alternate install prefix (advanced; default: /)
  --help, -h            Show this help text
EOF
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
        LOG_PIPE="${TMPDIR:-/tmp}/redroid-modules-install.$$.$$.pipe"
        rm -f "$LOG_PIPE"
        mkfifo "$LOG_PIPE"
        tee -a "$LOG_FILE" < "$LOG_PIPE" &
        TEE_PID=$!
        exec > "$LOG_PIPE" 2>&1
        rm -f "$LOG_PIPE"
        export TEE_PID LOG_PIPE
    else
        exec >> "$LOG_FILE" 2>&1
    fi

    export REDROID_INSTALL_LOG="$LOG_FILE"
    export REDROID_LOGGING_INITIALIZED=1
    printf 'Logging to %s\n' "$LOG_FILE"
}

print_summary() {
    echo
    echo "=================================================="
    echo "redroid-modules install summary"
    echo "=================================================="
    printf 'Kernel release : %s\n' "${KERNEL_RELEASE:-unknown}"
    printf 'Bundle version : %s\n' "$BUNDLE_VERSION"
    printf 'Install mode   : %s\n' "$INSTALL_MODE"
    printf 'Module load    : %s\n' "$MODULE_STATUS"
    printf 'binderfs       : %s\n' "$BINDERFS_STATUS"
    printf 'ipcverify      : %s\n' "$IPCVERIFY_STATUS"
    printf 'verify script  : %s\n' "$VERIFY_STATUS"
    printf 'Log file       : %s\n' "$LOG_FILE"
    echo "--------------------------------------------------"
    printf 'RESULT         : %s\n' "$SUMMARY_RESULT"
    echo "=================================================="
}

finish() {
    rc=$1
    trap - EXIT INT TERM HUP

    if [ "$rc" -ne 0 ] && [ "$SUMMARY_RESULT" = "SUCCESS" ]; then
        SUMMARY_RESULT=FAILURE
    fi

    print_summary

    if [ "${TEE_PID:-}" ]; then
        wait "$TEE_PID" 2>/dev/null || true
    fi

    exit "$rc"
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

require_root() {
    if [ "$(id -u)" != "0" ]; then
        echo "ERROR: root privileges are required to install kernel modules." >&2
        exit 1
    fi
}

prompt_continue() {
    if [ "$INTERACTIVE" -eq 0 ] || [ "$ASSUME_YES" -eq 1 ]; then
        return 0
    fi

    echo "This installer will configure binder_linux and ashmem_linux for kernel $KERNEL_RELEASE."
    printf 'Continue? [Y/n] '
    read ans
    case ${ans:-Y} in
        y|Y|yes|YES|'')
            return 0
            ;;
        *)
            SUMMARY_RESULT=ABORTED
            echo "Aborted by user."
            exit 1
            ;;
    esac
}

resolve_bundle_dir() {
    if [ -z "$BUNDLE_DIR" ]; then
        BUNDLE_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
    fi
    if [ -f "$BUNDLE_DIR/VERSION" ]; then
        BUNDLE_VERSION=$(tr -d ' \t\r\n' < "$BUNDLE_DIR/VERSION")
    fi
}

headers_present() {
    [ -d "/lib/modules/$KERNEL_RELEASE/build" ] \
        || [ -d "/usr/src/linux-headers-$KERNEL_RELEASE" ] \
        || [ -d "/usr/src/pve-headers-$KERNEL_RELEASE" ] \
        || [ -d "/usr/src/proxmox-headers-$KERNEL_RELEASE" ]
}

ensure_ca_certificates() {
    if command_exists apt-get; then
        apt-get update
        apt-get install -y ca-certificates
        update-ca-certificates || true
    elif command_exists dnf; then
        dnf install -y ca-certificates
        update-ca-trust extract || true
    elif command_exists yum; then
        yum install -y ca-certificates
        update-ca-trust extract || true
    elif command_exists zypper; then
        zypper --non-interactive install ca-certificates
        update-ca-certificates || true
    fi
}

install_build_requirements() {
    ensure_ca_certificates

    if command_exists apt-get; then
        apt-get update
        apt-get install -y dkms make gcc kmod \
            "linux-headers-$KERNEL_RELEASE" \
            || apt-get install -y dkms make gcc kmod "pve-headers-$KERNEL_RELEASE" \
            || apt-get install -y dkms make gcc kmod "proxmox-headers-$KERNEL_RELEASE" \
            || apt-get install -y dkms make gcc kmod linux-headers-amd64 \
            || apt-get install -y dkms make gcc kmod pve-headers
    elif command_exists dnf; then
        dnf install -y dkms make gcc kmod "kernel-devel-$KERNEL_RELEASE" \
            || dnf install -y dkms make gcc kmod kernel-devel
    elif command_exists yum; then
        yum install -y dkms make gcc kmod "kernel-devel-$KERNEL_RELEASE" \
            || yum install -y dkms make gcc kmod kernel-devel
    elif command_exists zypper; then
        zypper --non-interactive install dkms make gcc kmod "kernel-default-devel=$KERNEL_RELEASE" \
            || zypper --non-interactive install dkms make gcc kmod kernel-default-devel
    else
        echo "ERROR: no supported package manager found for DKMS fallback." >&2
        exit 1
    fi

    if ! command_exists dkms; then
        echo "ERROR: DKMS is still unavailable after dependency installation attempt." >&2
        exit 1
    fi

    if ! headers_present; then
        echo "ERROR: kernel headers for $KERNEL_RELEASE are not installed." >&2
        exit 1
    fi
}

install_prebuilt_modules() {
    module_dir="$BUNDLE_DIR/prebuilt/$KERNEL_RELEASE"
    target_root=${PREFIX%/}
    [ -n "$target_root" ] || target_root=/
    install_dir="$target_root/lib/modules/$KERNEL_RELEASE/updates/dkms"

    if [ ! -f "$module_dir/binder_linux.ko" ] || [ ! -f "$module_dir/ashmem_linux.ko" ]; then
        return 1
    fi

    echo "Installing matching prebuilt modules for kernel $KERNEL_RELEASE"
    mkdir -p "$install_dir"
    cp "$module_dir/binder_linux.ko" "$install_dir/binder_linux.ko"
    cp "$module_dir/ashmem_linux.ko" "$install_dir/ashmem_linux.ko"

    if command_exists depmod; then
        if [ "$target_root" = "/" ]; then
            depmod -a "$KERNEL_RELEASE"
        else
            depmod -b "$target_root" -a "$KERNEL_RELEASE"
        fi
    fi

    INSTALL_MODE=prebuilt
    return 0
}

dkms_install_one() {
    module_dir=$1
    package_name=$(sed -n 's/^PACKAGE_NAME="\([^"]*\)".*/\1/p' "$module_dir/dkms.conf" | head -n 1)
    package_version=$(sed -n 's/^PACKAGE_VERSION="\([^"]*\)".*/\1/p' "$module_dir/dkms.conf" | head -n 1)

    if [ -z "$package_name" ] || [ -z "$package_version" ]; then
        echo "ERROR: invalid dkms.conf in $module_dir" >&2
        exit 1
    fi

    echo "Registering DKMS module: $package_name $package_version"
    dkms add "$module_dir" >/dev/null 2>&1 || true
    dkms build -m "$package_name" -v "$package_version" -k "$KERNEL_RELEASE"
    dkms install -m "$package_name" -v "$package_version" -k "$KERNEL_RELEASE" --force
}

install_via_dkms() {
    if [ "$ALLOW_DKMS" -ne 1 ]; then
        echo "ERROR: no prebuilt module exists for $KERNEL_RELEASE and --no-dkms was supplied." >&2
        exit 1
    fi

    echo "No matching prebuilt module found for $KERNEL_RELEASE. Falling back to bundled DKMS source."
    install_build_requirements
    dkms_install_one "$BUNDLE_DIR/binder"
    dkms_install_one "$BUNDLE_DIR/ashmem"
    INSTALL_MODE=dkms
}

load_modules_and_mount() {
    echo "Loading redroid kernel modules"
    if [ -x "$BUNDLE_DIR/load_modules.sh" ]; then
        if sh "$BUNDLE_DIR/load_modules.sh"; then
            MODULE_STATUS=loaded
        else
            MODULE_STATUS=failed
            return 1
        fi
    else
        modprobe ashmem_linux
        modprobe binder_linux
        mkdir -p /dev/binderfs
        if ! grep -qs ' /dev/binderfs binder ' /proc/mounts; then
            mount -t binder binder /dev/binderfs
        fi
        MODULE_STATUS=loaded
    fi

    if grep -qs ' /dev/binderfs binder ' /proc/mounts; then
        BINDERFS_STATUS=mounted
    else
        BINDERFS_STATUS=not-mounted
        echo "ERROR: binderfs is not mounted at /dev/binderfs" >&2
        return 1
    fi
}

install_ipcverify_tools() {
    target_root=${PREFIX%/}
    [ -n "$target_root" ] || target_root=/
    tool_dir="$target_root/usr/local/lib/redroid-modules/bin"
    command_dir="$target_root/usr/local/bin"
    host_candidate=
    apk_candidate=

    mkdir -p "$tool_dir" "$command_dir"

    for candidate in \
        "$BUNDLE_DIR/bin/ipcverify-host" \
        "$BUNDLE_DIR/prebuilt/$KERNEL_RELEASE/ipcverify-host" \
        "$BUNDLE_DIR/ipcverify/build/ipcverify-host"; do
        if [ -x "$candidate" ]; then
            host_candidate=$candidate
            break
        fi
    done

    for candidate in \
        "$BUNDLE_DIR/bin/ipcverify.apk" \
        "$BUNDLE_DIR/prebuilt/$KERNEL_RELEASE/ipcverify.apk" \
        "$BUNDLE_DIR/ipcverify/apk/ipcverify.apk"; do
        if [ -f "$candidate" ]; then
            apk_candidate=$candidate
            break
        fi
    done

    if [ -n "$host_candidate" ]; then
        cp "$host_candidate" "$tool_dir/ipcverify-host"
        chmod 0755 "$tool_dir/ipcverify-host"
        ln -sf ../lib/redroid-modules/bin/ipcverify-host "$command_dir/ipcverify-host"
    fi

    if [ -n "$apk_candidate" ]; then
        cp "$apk_candidate" "$tool_dir/ipcverify.apk"
        chmod 0644 "$tool_dir/ipcverify.apk"
    fi
}

run_ipcverify_suite() {
    if [ "$SKIP_TEST" -eq 1 ]; then
        IPCVERIFY_STATUS=skipped
        return 0
    fi

    echo "Running installed ipcverify-host validation"
    target_root=${PREFIX%/}
    [ -n "$target_root" ] || target_root=/
    host_bin=

    for candidate in \
        "$target_root/usr/local/bin/ipcverify-host" \
        "$target_root/usr/local/lib/redroid-modules/bin/ipcverify-host" \
        "$BUNDLE_DIR/bin/ipcverify-host"; do
        if [ -x "$candidate" ]; then
            host_bin=$candidate
            break
        fi
    done

    if [ -z "$host_bin" ]; then
        IPCVERIFY_STATUS=failed
        echo "ERROR: ipcverify-host is missing from the installed bundle." >&2
        return 1
    fi

    if "$host_bin" --local-only --yes; then
        IPCVERIFY_STATUS=passed
        return 0
    fi

    IPCVERIFY_STATUS=failed
    return 1
}

run_verify_script() {
    if [ -f "$BUNDLE_DIR/scripts/verify-environment.sh" ] && command_exists bash; then
        echo "Running bundled environment verification"
        if (cd "$BUNDLE_DIR" && bash ./scripts/verify-environment.sh --format keyvalue); then
            VERIFY_STATUS=passed
            return 0
        fi
        VERIFY_STATUS=failed
        return 1
    fi

    VERIFY_STATUS=skipped
    return 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --bundle-dir)
            BUNDLE_DIR=$2
            shift 2
            ;;
        --yes|-y)
            ASSUME_YES=1
            shift
            ;;
        --non-interactive)
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

trap 'finish $?' EXIT
trap 'SUMMARY_RESULT=ABORTED; exit 1' INT TERM HUP

setup_logging
resolve_bundle_dir
require_root

KERNEL_RELEASE=$(uname -r)

if [ ! -d "$BUNDLE_DIR/prebuilt" ] || [ ! -d "$BUNDLE_DIR/binder" ] || [ ! -d "$BUNDLE_DIR/ashmem" ]; then
    echo "ERROR: bundle directory '$BUNDLE_DIR' is missing required installer content." >&2
    exit 1
fi

prompt_continue

echo "Installing redroid-modules for kernel $KERNEL_RELEASE"

if ! install_prebuilt_modules; then
    install_via_dkms
fi

install_ipcverify_tools

if [ "$PREFIX" != "/" ]; then
    MODULE_STATUS=staged-only
    BINDERFS_STATUS=staged-only
    IPCVERIFY_STATUS=skipped
    VERIFY_STATUS=skipped
    echo "Alternate prefix mode requested; files were staged under $PREFIX without loading live modules."
    exit 0
fi

load_modules_and_mount
run_ipcverify_suite
run_verify_script

echo "redroid-modules installation completed successfully."
SUMMARY_RESULT=SUCCESS
