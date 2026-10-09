#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${CC:=cc}"
# GEDIT_INCLUDE_DIR supports development against a locally installed fork.
if pkg-config --exists gedit; then
    flags=$(pkg-config --cflags gedit)
else
    : "${GEDIT_INCLUDE_DIR:?Install gedit-devel or set GEDIT_INCLUDE_DIR to the directory containing gedit/}"
    flags="-I$GEDIT_INCLUDE_DIR"
fi
mkdir -p build
$CC -Wall -Wextra -Wno-unused-parameter -fPIC -shared $flags \
    $(pkg-config --cflags gtk+-3.0 libpeas-1.0 libgedit-tepl-6) \
    git-status.c git-query.c -o build/libgit-status.so \
    $(pkg-config --libs gtk+-3.0 libpeas-1.0 libgedit-tepl-6)
cp git-status.plugin build/
