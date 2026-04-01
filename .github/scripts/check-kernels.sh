#!/usr/bin/env bash
# check-kernels.sh — Query distro package repositories for the latest
# available kernel-header packages and compare against known versions.
#
# Output lines prefixed with "NEW:" indicate previously-unseen kernels.
# Exit 0 always (reporting tool, not a gate).
set -euo pipefail

KNOWN_FILE=".github/kernel-versions.json"
REPORT=""

add_line() { REPORT="$REPORT$1"$'\n'; }

# Load known versions (JSON array of strings)
if [ -f "$KNOWN_FILE" ]; then
    KNOWN=$(cat "$KNOWN_FILE")
else
    KNOWN="[]"
fi

is_known() {
    echo "$KNOWN" | grep -qF "\"$1\""
}

add_line "=== Kernel Update Check $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
add_line ""

# --- Ubuntu ---
check_ubuntu() {
    local release="$1"
    local codename="$2"
    add_line "--- Ubuntu $release ($codename) ---"

    # Query the package index for linux-headers-generic
    local url="https://packages.ubuntu.com/$codename/amd64/linux-headers-generic"
    local version
    version=$(curl -sfL "$url" 2>/dev/null \
        | grep -oP 'linux-headers-[0-9]+\.[0-9]+\.[0-9]+-[0-9]+' \
        | sort -Vu | tail -1 || echo "")

    if [ -n "$version" ]; then
        if is_known "$version"; then
            add_line "  $version (known)"
        else
            add_line "NEW: $version (Ubuntu $release)"
        fi
    else
        add_line "  (could not query Ubuntu $release)"
    fi
}

# --- Debian ---
check_debian() {
    local release="$1"
    add_line "--- Debian $release ---"

    local url="https://packages.debian.org/$release/amd64/linux-headers-amd64"
    local version
    version=$(curl -sfL "$url" 2>/dev/null \
        | grep -oP 'linux-headers-[0-9]+\.[0-9]+\.[0-9]+-[a-z0-9]+' \
        | sort -Vu | tail -1 || echo "")

    if [ -n "$version" ]; then
        if is_known "$version"; then
            add_line "  $version (known)"
        else
            add_line "NEW: $version (Debian $release)"
        fi
    else
        add_line "  (could not query Debian $release)"
    fi
}

# --- Proxmox VE ---
check_proxmox() {
    local label="$1"
    local suite="$2"
    add_line "--- Proxmox VE $label ($suite) ---"

    # Query the Proxmox package repo for pve-headers
    local url="http://download.proxmox.com/debian/pve/dists/$suite/pve-no-subscription/binary-amd64/Packages.gz"
    local versions
    versions=$(curl -sfL "$url" 2>/dev/null \
        | zcat 2>/dev/null \
        | grep -oP 'pve-headers-[0-9]+\.[0-9]+\.[0-9]+-[0-9]+-pve' \
        | sort -Vu | tail -3 || echo "")

    if [ -n "$versions" ]; then
        while IFS= read -r v; do
            if is_known "$v"; then
                add_line "  $v (known)"
            else
                add_line "NEW: $v (Proxmox VE $label)"
            fi
        done <<< "$versions"
    else
        add_line "  (could not query Proxmox VE $label repo)"
    fi
}

check_ubuntu "24.04" "noble"
check_ubuntu "22.04" "jammy"
check_debian "bookworm"
check_debian "trixie"
check_proxmox "8" "bookworm"
check_proxmox "9" "trixie"

add_line ""
add_line "=== End ==="

echo "$REPORT"
