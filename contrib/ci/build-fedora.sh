#!/bin/sh
set -e
export LC_ALL=C.UTF-8
mkdir -p build && cd build
rm * -rf
meson .. \
    -Db_coverage=true \
    -Dgtkdoc=true \
    -Dtests=true $@
ninja -v || bash
ninja test -v
ninja coverage-text
cat meson-logs/coverage.txt
DESTDIR=/tmp/install-ninja ninja install
cd ..
