#!/bin/sh
# Rebuilds the export-enumeration test libraries (x86-64 Linux, gcc + GNU ld).
# The .so files are checked in, so this is only needed after changing exports.c.
set -e
cd "$(dirname "$0")"
for style in gnu sysv; do
    gcc -shared -fPIC -O1 -nostdlib -Wl,--version-script=exports.map \
        -Wl,--hash-style=$style -Wl,--build-id=none \
        -o ../libexports_$style.so exports.c
done
