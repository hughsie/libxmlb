#!/bin/sh

set -o errexit
set -o nounset
set -o pipefail


export LC_ALL=C.UTF-8

COMMIT=${1}
LATEST_RELEASE=${2}

build_install() {
    name=${1}

    echo "# Building the ${name} version…"

    rm -rf _build ${name}
    meson _build \
        --prefix=/usr \
	--libdir=lib \
        -Db_coverage=false \
        -Dgtkdoc=false \
        -Dtests=false
    ninja -v -C _build
    DESTDIR=${PWD}/${name} ninja -v -C _build install
}


git checkout -q ${LATEST_RELEASE}
build_install "reference"

git checkout -q ${COMMIT}
build_install "new"

abidiff --headers-dir1 "reference/usr/include" --headers-dir2 "new/usr/include" \
    --drop-private-types --fail-no-debug-info --no-added-syms \
    reference/usr/lib/libxmlb.so new/usr/lib/libxmlb.so
