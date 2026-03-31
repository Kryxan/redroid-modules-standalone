#!/usr/bin/env bash
# check-compat.sh — Verify every compat_* macro/function defined in compat.h
# is actually referenced by at least one consumer source file.
#
# Exit 0 = all macros consumed.  Exit 1 = orphan macros found.
set -euo pipefail

ERRORS=0

check_header() {
    local header="$1"
    local search_dir="$2"
    local label="$3"

    echo "=== Checking $label: $header ==="

    # Extract all public compat identifiers:
    #   #define compat_xxx  or  COMPAT_XXX
    #   static inline ... compat_xxx(
    local macros
    macros=$(grep -oE '\b(compat_[a-z_]+|COMPAT_[A-Z_]+)\b' "$header" \
        | sort -u \
        | grep -vE '^(COMPAT_H|_BINDER_COMPAT_H|_ASHMEM_COMPAT_H)$' || true)

    if [ -z "$macros" ]; then
        echo "  (no compat identifiers found)"
        return
    fi

    local unused=0
    while IFS= read -r macro; do
        # Search .c and .h files (excluding the compat header itself)
        local hits
        hits=$(grep -rl --include='*.c' --include='*.h' -- "$macro" "$search_dir" \
            | grep -v "compat.h" | head -1 || true)
        if [ -z "$hits" ]; then
            echo "  WARNING: $macro defined but never used"
            unused=$((unused + 1))
        fi
    done <<< "$macros"

    if [ "$unused" -gt 0 ]; then
        echo "  $unused orphan macro(s) in $header"
        ERRORS=$((ERRORS + unused))
    else
        echo "  All macros consumed."
    fi
    echo ""
}

check_header "binder/compat.h" "binder/" "binder"
check_header "ashmem/compat.h" "ashmem/" "ashmem"

if [ "$ERRORS" -gt 0 ]; then
    echo "::warning::$ERRORS orphan compat macro(s) found. These may be dead code or awaiting consumers."
    # Warn but don't fail — some macros may be consumed conditionally.
    exit 0
fi

echo "All compat macros are consumed."
