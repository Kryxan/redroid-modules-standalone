#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
VERSION=$(sh "$SCRIPT_DIR/read-version.sh")
EXPECTED_TAG="v$VERSION"

if [ $# -gt 0 ]; then
    ACTUAL_TAG=$1
    if [ "$ACTUAL_TAG" != "$EXPECTED_TAG" ]; then
        echo "ERROR: tag '$ACTUAL_TAG' does not match VERSION '$VERSION' (expected '$EXPECTED_TAG')." >&2
        exit 1
    fi
fi

printf '%s\n' "$VERSION"
