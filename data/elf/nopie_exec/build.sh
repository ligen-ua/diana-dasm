#!/bin/sh
# Rebuilds the non-PIE test executable (Linux x86-64, gcc + GNU ld).
# The binary is checked in, so this is only needed after changing main.c.
# -rdynamic puts nopie_func_* into .dynsym, which is what Orthia reads as exports.
set -e
cd "$(dirname "$0")"
gcc -no-pie -fno-pie -O1 -rdynamic -Wl,--build-id=none -o ../nopie_x64 main.c
