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

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi
if [[ -n "${CMAKE_BUILD_RPATH:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_BUILD_RPATH="$CMAKE_BUILD_RPATH")
fi
if [[ -n "${CMAKE_EXE_LINKER_FLAGS:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS="$CMAKE_EXE_LINKER_FLAGS")
fi

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    cmake -S "$CLIENT" -B "$CLIENT/build-release" "${CMAKE_ARGS[@]}"
    cmake --build "$CLIENT/build-release" -j"$(nproc)"
fi

BINARY="$CLIENT/build-release/opensource-communicator"
if [[ ! -x "$BINARY" ]]; then
    echo "Binary not found: $BINARY" >&2
    exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications"

install -m755 "$BINARY" "$APPDIR/usr/bin/"
install -m644 "$ROOT/packaging/linux/opensource-communicator.desktop" "$APPDIR/usr/share/applications/"
for _icon_size in 16 32 48 64 128 256 512; do
    _icon_dir="$APPDIR/usr/share/icons/hicolor/${_icon_size}x${_icon_size}/apps"
    mkdir -p "$_icon_dir"
    install -m644 "$ROOT/packaging/linux/icons/opensource-communicator-${_icon_size}.png" \
        "$_icon_dir/opensource-communicator.png"
done
# Top-level icon used by linuxdeploy for the AppImage thumbnail
install -m644 "$ROOT/packaging/linux/icons/opensource-communicator-256.png" \
    "$APPDIR/opensource-communicator.png"

export QMAKE="$(command -v qmake6 || command -v qmake)"
export VERSION
export ARCH=x86_64
export NO_STRIP=1
export APPIMAGE_EXTRACT_AND_RUN=1
export LINUXDEPLOY_PLUGIN_DIR="$TOOLS"
export OUTPUT="$DIST/OpenSource-Communicator-${VERSION}-x86_64.AppImage"

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    export LD_LIBRARY_PATH="${CMAKE_PREFIX_PATH}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

LIB_ARGS=()
while IFS= read -r lib_path; do
    if [[ -f "$lib_path" ]]; then
        LIB_ARGS+=(--library "$lib_path")
    fi
done < <(ldd "$BINARY" | awk '/libdatachannel|libopus/ {print $3}')

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
