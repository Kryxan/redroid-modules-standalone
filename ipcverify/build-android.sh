#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH='' && cd -- "$(dirname -- "$0")" && pwd)
OUTPUT_DIR=${OUTPUT_DIR:-$PROJECT_DIR/../bin}
GRADLE_CMD=
GRADLE_VERSION=8.7
GRADLE_DIST_DIR="$PROJECT_DIR/.gradle-dist"

case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS_NT*)
        echo "ERROR: Windows hosts are not supported for building this project." >&2
        echo "Use a Linux host or Linux CI." >&2
        exit 1
        ;;
esac

if [ -z "${JAVA_HOME:-}" ] && command -v java >/dev/null 2>&1; then
    JAVA_BIN=$(command -v java)
    JAVA_HOME_CANDIDATE=$(CDPATH='' cd -- "$(dirname -- "$JAVA_BIN")/.." 2>/dev/null && pwd -P || true)
    if [ -n "$JAVA_HOME_CANDIDATE" ] && [ -x "$JAVA_HOME_CANDIDATE/bin/java" ]; then
        export JAVA_HOME="$JAVA_HOME_CANDIDATE"
    fi
fi

if [ -z "${ANDROID_SDK_ROOT:-}" ] && [ -n "${ANDROID_HOME:-}" ]; then
    export ANDROID_SDK_ROOT="$ANDROID_HOME"
fi

if [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    echo "ERROR: ANDROID_SDK_ROOT is not set." >&2
    echo "Set ANDROID_SDK_ROOT (or ANDROID_HOME) to your Android SDK location before building the APK." >&2
    exit 1
fi

if [ -x "$PROJECT_DIR/gradlew" ]; then
    GRADLE_CMD="$PROJECT_DIR/gradlew"
elif command -v gradle >/dev/null 2>&1; then
    GRADLE_CMD=gradle
else
    mkdir -p "$GRADLE_DIST_DIR"
    if [ ! -x "$GRADLE_DIST_DIR/gradle-$GRADLE_VERSION/bin/gradle" ]; then
        ARCHIVE="$GRADLE_DIST_DIR/gradle-$GRADLE_VERSION-bin.zip"
        URL="https://services.gradle.org/distributions/gradle-$GRADLE_VERSION-bin.zip"
        echo "Downloading Gradle $GRADLE_VERSION..."
        if command -v curl >/dev/null 2>&1; then
            curl -fsSL "$URL" -o "$ARCHIVE"
        elif command -v wget >/dev/null 2>&1; then
            wget -O "$ARCHIVE" "$URL"
        else
            echo "ERROR: neither curl nor wget is available to download Gradle." >&2
            exit 1
        fi
        unzip -oq "$ARCHIVE" -d "$GRADLE_DIST_DIR"
    fi
    GRADLE_CMD="$GRADLE_DIST_DIR/gradle-$GRADLE_VERSION/bin/gradle"
fi

SDK_DIR_ESCAPED=$(printf '%s' "$ANDROID_SDK_ROOT" | sed 's#\\#\\\\#g')
cat > "$PROJECT_DIR/local.properties" <<EOF
sdk.dir=${SDK_DIR_ESCAPED}
EOF

"$GRADLE_CMD" -p "$PROJECT_DIR" assembleDebug

APK_SOURCE="$PROJECT_DIR/app/build/outputs/apk/debug/ipcverify.apk"
if [ ! -f "$APK_SOURCE" ]; then
    echo "ERROR: Android build finished but ipcverify.apk was not produced." >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
cp -f "$APK_SOURCE" "$OUTPUT_DIR/ipcverify.apk"

printf '%s\n' "$OUTPUT_DIR/ipcverify.apk"
