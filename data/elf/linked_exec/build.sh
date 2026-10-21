#!/bin/sh
# Rebuilds the dependency-linking test executable (Linux x86-64, gcc + GNU ld).
# The binary is checked in, so this is only needed after changing main.c.
# -l:libexports_gnu.so makes DT_NEEDED the bare name, so Orthia finds the library beside it;
# -z lazy keeps the imports as PLT (JUMP_SLOT) relocations.
set -e
cd "$(dirname "$0")"
gcc -fPIE -pie -nostdlib -O1 -Wl,-z,lazy -Wl,--build-id=none -Wl,--allow-shlib-undefined \
    main.c -L.. -l:libexports_gnu.so -o ../linked_x64
