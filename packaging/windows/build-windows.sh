#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CLIENT="$ROOT/client"
DIST="$ROOT/dist"
MSYS="$ROOT/packaging/msys2"
DEPS="$MSYS/deps"
VERSION="$(grep -m1 'project(opensource-communicator VERSION' "$CLIENT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ARCHIVE="$DIST/OpenSource-Communicator-${VERSION}-win64.zip"

mkdir -p "$DIST" "$MSYS" "$DEPS"

if [[ ! -x "$MSYS/root/usr/bin/bash" ]]; then
    echo "Installing MSYS2..."
    curl -fsSL -o "$MSYS/msys2-base.tar.xz" \
        "https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.tar.xz"
    mkdir -p "$MSYS/root"
    tar -xJf "$MSYS/msys2-base.tar.xz" -C "$MSYS/root" --strip-components=1
fi

MSYS_BASH="$MSYS/root/usr/bin/bash"

"$MSYS_BASH" -lc "
set -euo pipefail
pacman -Sy --noconfirm --needed \
    git \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-websockets \
    mingw-w64-ucrt-x86_64-qt6-multimedia \
    mingw-w64-ucrt-x86_64-opus \
    mingw-w64-ucrt-x86_64-openssl \
    zip
"

"$MSYS_BASH" -lc "
set -euo pipefail
export PATH=/ucrt64/bin:\$PATH
PREFIX='$DEPS/prefix'
if [[ ! -f \"\$PREFIX/lib/cmake/LibDataChannel/LibDataChannelConfig.cmake\" ]]; then
    rm -rf '$DEPS/libdatachannel-src' '$DEPS/libdatachannel-build'
    git clone --depth 1 --branch v0.24.5 https://github.com/paullouisageneau/libdatachannel.git '$DEPS/libdatachannel-src'
    cmake -S '$DEPS/libdatachannel-src' -B '$DEPS/libdatachannel-build' -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=\"\$PREFIX\" \
        -DUSE_GNUTLS=0 \
        -DUSE_MBEDTLS=0 \
        -DUSE_NICE=0 \
        -DNO_EXAMPLES=1 \
        -DNO_TESTS=1
    cmake --build '$DEPS/libdatachannel-build'
    cmake --install '$DEPS/libdatachannel-build'
fi
"

"$MSYS_BASH" -lc "
set -euo pipefail
export PATH=/ucrt64/bin:\$PATH
PREFIX='$DEPS/prefix'
cmake -S '$CLIENT' -B '$CLIENT/build-win64' -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=\"\$PREFIX;/ucrt64\"
cmake --build '$CLIENT/build-win64'
"

STAGE="$CLIENT/build-win64/portable"
rm -rf "$STAGE"
mkdir -p "$STAGE"

"$MSYS_BASH" -lc "
set -euo pipefail
export PATH=/ucrt64/bin:\$PATH
PREFIX='$DEPS/prefix'
EXE='$CLIENT/build-win64/opensource-communicator.exe'
STAGE='$STAGE'
cp \"\$EXE\" \"\$STAGE/\"
windeployqt6 --no-translations --no-system-d3d-compiler --no-opengl-sw \"\$STAGE/opensource-communicator.exe\"
cp \"\$PREFIX/bin/libdatachannel.dll\" \"\$STAGE/\"
cp /ucrt64/bin/libopus-0.dll \"\$STAGE/\"
"

rm -f "$ARCHIVE"
(cd "$STAGE" && zip -r9 "$ARCHIVE" .)
echo "Windows package: $ARCHIVE"
