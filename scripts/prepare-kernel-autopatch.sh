#!/bin/sh
set -eu

if [ $# -lt 2 ]; then
    echo "Usage: $(basename "$0") <artifact-dir> <output-file>" >&2
    exit 2
fi

ARTIFACT_DIR=$1
OUTPUT_FILE=$2
SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
VERSION=$(sh "$SCRIPT_DIR/read-version.sh")
GENERATED_AT=$(date -u +%Y-%m-%dT%H:%M:%SZ)
TMP_ERRORS=$(mktemp)
trap 'rm -f "$TMP_ERRORS"' EXIT HUP INT TERM

find "$ARTIFACT_DIR" -type f \( -name '*.log' -o -name '*.txt' \) | sort | while read -r file; do
    printf '===== %s =====\n' "$file" >> "$TMP_ERRORS"
    if ! grep -E 'error:|fatal:|undefined reference|implicit declaration|No rule to make target|FAILED' "$file" | head -n 160 >> "$TMP_ERRORS"; then
        sed -n '1,120p' "$file" >> "$TMP_ERRORS"
    fi
    printf '\n' >> "$TMP_ERRORS"
done

if [ ! -s "$TMP_ERRORS" ]; then
    printf 'No compiler diagnostics were extracted from %s\n' "$ARTIFACT_DIR" > "$TMP_ERRORS"
fi

cat > "$OUTPUT_FILE" <<EOF
## Kernel compatibility autopatch request

- Repository version: \`$VERSION\`
- Generated at: \`$GENERATED_AT\`
- Target branch: \`prerelease\`

This request was generated automatically after a kernel-triggered rebuild failed.
Please keep the semantic project version unchanged and stage any compatibility fix on
\`prerelease\` first.

### Guardrails

1. Preserve the planned compatibility scaffolding documented in \`docs/PLANNED_CHANGES.md\`.
2. Do not remove \`ashmem_backing_mode\`, related debug hooks, or runtime tests.
3. Limit the fix to kernel compatibility shims, CI adjustments, or safe build/runtime changes.
4. Re-run \`make ci-check\`, \`make ci-test\`, and the failing kernel matrix entry after patching.

### Compiler errors

\`\`\`text
$(cat "$TMP_ERRORS")\`\`\`
EOF
