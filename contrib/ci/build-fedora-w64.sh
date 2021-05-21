#!/bin/sh
set -e
export LC_ALL=C.UTF-8
mkdir -p build && cd build
rm -rf *
meson .. \
    --cross-file=../contrib/mingw64.cross \
    -Dintrospection=false \
    -Dgtkdoc=false \
    -Dtests=true $@
ninja -v || bash
wine reg add "HKEY_CURRENT_USER\Environment" /v PATH /d /usr/x86_64-w64-mingw32/sys-root/mingw/bin
ninja test -v
DESTDIR=/tmp/install-ninja ninja install
cd ..
