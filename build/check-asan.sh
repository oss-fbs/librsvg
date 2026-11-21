#!/bin/sh
# build/check-asan.sh: ASan+UBSan configure, build, and make check
# in a source snapshot under build-asan/.
#
# Copyright (C) 2026 Randy Butler
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this program; if not, see LICENCE-LIB.
#
# Does not install. Does not touch in-tree objects. An in-tree
# `./configure` forbids VPATH, so this copies tracked sources into
# build-asan/ (gitignored via /build-*/) and configures that copy.

set -eu

src=${1:-}
if [ -z "$src" ]; then
    src=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
fi
src=$(CDPATH= cd -- "$src" && pwd)

case "$src" in
    */build-asan)
        echo "$0: refuse to run from inside build-asan" >&2
        exit 1
        ;;
esac

if [ ! -x "$src/configure" ]; then
    echo "$0: $src/configure is missing or not executable" >&2
    exit 1
fi

cc=${CC:-cc}
if ! "$cc" -fsanitize=address,undefined -c -x c /dev/null -o /dev/null 2>/dev/null; then
    echo "$0: $cc does not support -fsanitize=address,undefined" >&2
    exit 1
fi

# ASan must be first in the link map. Desktop LD_PRELOAD (gtk3-nocsd)
# makes configure's run-test fail with "runtime does not come first".
unset LD_PRELOAD

build=$src/build-asan
mkdir -p "$build"

echo "$0: snapshot sources -> $build"
if git -C "$src" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$src" ls-files -z \
        | rsync -a --files-from=- --from0 "$src"/ "$build"/
else
    rsync -a \
        --exclude '/build-asan/' \
        --exclude '/.git/' \
        --exclude '/stage/' \
        --exclude '.libs/' \
        --exclude '.deps/' \
        --exclude '*.o' \
        --exclude '*.lo' \
        --exclude '*.la' \
        --exclude 'config.status' \
        --exclude 'config.log' \
        --exclude 'config.h' \
        --exclude 'libtool' \
        --exclude 'stamp-h1' \
        --exclude 'Makefile' \
        "$src"/ "$build"/
fi
chmod +x "$build/configure" "$build/build/check-asan.sh" 2>/dev/null || true

asan_cflags="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
asan_ldflags="-fsanitize=address,undefined"
# glibc MALLOC_CHECK_ fights ASan. GLib/Cairo/Pango are not instrumented.
asan_env="G_TEST_SRCDIR=$build/tests G_TEST_BUILDDIR=$build/tests G_DEBUG=gc-friendly ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:allocator_may_return_null=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"

if [ ! -f "$build/config.status" ]; then
    echo "$0: configuring $build"
    ( cd "$build" && ./configure --prefix=/usr \
        --disable-introspection \
        --disable-vala \
        --disable-maintainer-mode \
        --disable-Bsymbolic \
        CFLAGS="$asan_cflags" \
        LDFLAGS="$asan_ldflags" )
fi

jobs=$(nproc 2>/dev/null || echo 1)
echo "$0: make -j$jobs all (in $build)"
make -C "$build" -j"$jobs" all

echo "$0: make check (ASan+UBSan, no install)"
make -C "$build" check TESTS_ENVIRONMENT="$asan_env"
