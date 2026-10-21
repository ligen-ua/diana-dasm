#!/bin/sh
# Rebuilds the startup-routine test libraries (Linux, gcc + GNU ld; aarch64-linux-gnu-gcc for arm64,
# arm-linux-gnueabihf-gcc for arm32: Thumb by default, -marm for A32).
# The .so files are checked in, so this is only needed after changing startup.c.
set -e
cd "$(dirname "$0")"
FLAGS="-shared -fPIC -O1 -nostdlib -Wl,-init,startup_init -Wl,--build-id=none"
gcc $FLAGS -o ../libstartup_x64.so startup.c
aarch64-linux-gnu-gcc $FLAGS -o ../libstartup_arm64.so startup.c
arm-linux-gnueabihf-gcc $FLAGS -o ../libstartup_thumb.so startup.c
arm-linux-gnueabihf-gcc $FLAGS -marm -o ../libstartup_arm32.so startup.c
