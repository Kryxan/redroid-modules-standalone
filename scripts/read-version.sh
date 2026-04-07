#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH='' && cd -- "$SCRIPT_DIR/.." && pwd)
VERSION_FILE=${VERSION_FILE:-$ROOT_DIR/VERSION}
MODE=${1:---plain}

if [ ! -f "$VERSION_FILE" ]; then
    echo "ERROR: VERSION file not found at $VERSION_FILE" >&2
    exit 1
fi

VERSION=$(tr -d ' \t\r\n' < "$VERSION_FILE")

if ! printf '%s\n' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: VERSION must contain only X.Y.Z, found '$VERSION'" >&2
    exit 1
fi

case "$MODE" in
    --plain|plain|'')
        printf '%s\n' "$VERSION"
        ;;
    --tag|tag)
        printf 'v%s\n' "$VERSION"
        ;;
    *)
        echo "Usage: $(basename "$0") [--plain|--tag]" >&2
        exit 2
        ;;
esac
