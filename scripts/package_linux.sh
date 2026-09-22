#!/usr/bin/env bash
# scripts/package_linux.sh
# Automated Linux Production Packaging & Deployment Script for NeoNect

set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
OUTPUT_DIR="${3:-dist}"

echo "=========================================================="
echo "  NEONECT DESKTOP - LINUX PRODUCTION PACKAGER            "
echo "=========================================================="

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# 1. Generate release notes and patch metadata
echo "➔ [1/5] Generating Release Notes and Patch Notes..."
python3 scripts/generate_release_notes.py || python scripts/generate_release_notes.py

# 2. Build Release Binaries
echo "➔ [2/5] Building NeoNectApp ($CONFIG)..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG" --target NeoNectApp NeoNectTests

# 3. Run Automated Tests
echo "➔ [3/5] Running All Test Suites..."
if [ -f "$BUILD_DIR/NeoNectTests" ]; then
    "$BUILD_DIR/NeoNectTests"
    echo "  ✔ All tests passed 100%!"
fi

# 4. Construct AppDir & Directory Layout
echo "➔ [4/5] Staging Linux AppDir layout..."
APPDIR="$BUILD_DIR/package_linux_appdir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/metainfo"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$APPDIR/usr/share/doc/neonect"

# Install binary and desktop files
cp "$BUILD_DIR/NeoNectApp" "$APPDIR/usr/bin/NeoNectApp"
chmod +x "$APPDIR/usr/bin/NeoNectApp"
cp "packaging/linux/io.neonect.NeoNect.desktop" "$APPDIR/usr/share/applications/"
cp "packaging/linux/io.neonect.NeoNect.metainfo.xml" "$APPDIR/usr/share/metainfo/"
if [ -f "assets/NeoNect/icon.png" ]; then
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
    cp "assets/NeoNect/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.neonect.NeoNect.png"
    cp "assets/NeoNect/icon.png" "$APPDIR/io.neonect.NeoNect.png"
fi
cp "RELEASE_NOTES.md" "$APPDIR/usr/share/doc/neonect/" || true
cp "CHANGELOG.md" "$APPDIR/usr/share/doc/neonect/" || true

# AppRun launcher
cat << 'EOF' > "$APPDIR/AppRun"
#!/bin/sh
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QML_IMPORT_PATH="${HERE}/usr/qml:${QML_IMPORT_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
exec "${HERE}/usr/bin/NeoNectApp" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# 5. Create Standalone Tarball
echo "➔ [5/5] Creating Standalone Distribution Tarball..."
mkdir -p "$OUTPUT_DIR"
VERSION="${NEONECT_VERSION:-1.0.0}"
VERSION="${VERSION#v}"
TARBALL_NAME="NeoNect-v${VERSION}-linux-x86_64.tar.gz"
TARBALL_PATH="$OUTPUT_DIR/$TARBALL_NAME"

tar -czf "$TARBALL_PATH" -C "$APPDIR" .

echo "=========================================================="
echo "  ✔ PACKAGE CREATED SUCCESSFULLY: $TARBALL_PATH"
echo "=========================================================="
