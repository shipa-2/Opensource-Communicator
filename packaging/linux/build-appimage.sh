#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CLIENT="$ROOT/client"
DIST="$ROOT/dist"
TOOLS="$ROOT/packaging/tools"
APPDIR="$CLIENT/build-appimage/AppDir"
VERSION="$(grep -m1 'project(opensource-communicator VERSION' "$CLIENT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"

mkdir -p "$DIST" "$TOOLS"

if [[ ! -x "$TOOLS/linuxdeploy-x86_64.AppImage" ]]; then
    curl -fsSL -o "$TOOLS/linuxdeploy-x86_64.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
    chmod +x "$TOOLS/linuxdeploy-x86_64.AppImage"
fi

if [[ ! -x "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" ]]; then
    curl -fsSL -o "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"
fi

cmake -S "$CLIENT" -B "$CLIENT/build-release" -DCMAKE_BUILD_TYPE=Release
cmake --build "$CLIENT/build-release" -j"$(nproc)"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/scalable/apps"

install -m755 "$CLIENT/build-release/opensource-communicator" "$APPDIR/usr/bin/"
install -m644 "$ROOT/packaging/linux/opensource-communicator.desktop" "$APPDIR/usr/share/applications/"
install -m644 "$ROOT/packaging/linux/opensource-communicator.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/"

export QMAKE="$(command -v qmake6 || command -v qmake)"
export VERSION
export ARCH=x86_64
export NO_STRIP=1
export APPIMAGE_EXTRACT_AND_RUN=1
export LINUXDEPLOY_PLUGIN_DIR="$TOOLS"
export OUTPUT="$DIST/OpenSource-Communicator-${VERSION}-x86_64.AppImage"

if [[ -d "$HOME/deps/lib" ]]; then
  export LD_LIBRARY_PATH="$HOME/deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

LIB_ARGS=()
while IFS= read -r lib_path; do
  if [[ -f "$lib_path" ]]; then
    LIB_ARGS+=(--library "$lib_path")
  fi
done < <(ldd "$CLIENT/build-release/opensource-communicator" | awk '/libdatachannel|libopus/ {print $3}')

cd "$CLIENT/build-appimage"
"$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/opensource-communicator.desktop" \
    --executable "$APPDIR/usr/bin/opensource-communicator" \
    "${LIB_ARGS[@]}" \
    --plugin qt \
    --output appimage

mv -f ./*.AppImage "$OUTPUT" 2>/dev/null || true
if [[ ! -f "$OUTPUT" ]]; then
  found="$(find . -maxdepth 1 -name '*.AppImage' -print -quit)"
  if [[ -n "$found" ]]; then
    mv -f "$found" "$OUTPUT"
  fi
fi
if [[ ! -f "$OUTPUT" ]]; then
    echo "AppImage build failed: $OUTPUT not found" >&2
    exit 1
fi
echo "AppImage: $OUTPUT"
