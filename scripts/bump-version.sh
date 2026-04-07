#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH='' && cd -- "$SCRIPT_DIR/.." && pwd)
PART=${1:-patch}
VERSION=$(sh "$SCRIPT_DIR/read-version.sh")

old_ifs=$IFS
IFS=.
set -- $VERSION
IFS=$old_ifs

MAJOR=$1
MINOR=$2
PATCH=$3

case "$PART" in
    major)
        MAJOR=$((MAJOR + 1))
        MINOR=0
        PATCH=0
        ;;
    minor)
        MINOR=$((MINOR + 1))
        PATCH=0
        ;;
    patch)
        PATCH=$((PATCH + 1))
        ;;
    *)
        echo "Usage: $(basename "$0") [major|minor|patch]" >&2
        exit 2
        ;;
esac

NEXT_VERSION="${MAJOR}.${MINOR}.${PATCH}"
printf '%s\n' "$NEXT_VERSION" > "$ROOT_DIR/VERSION"
printf '%s\n' "$NEXT_VERSION"
