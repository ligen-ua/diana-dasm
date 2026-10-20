#!/bin/sh
# Rebuilds the TLS callback test DLLs (clang + lld-link, no CRT, no Windows SDK needed).
# The .dll files are checked in, so this is only needed after changing tls.c.
set -e
cd "$(dirname "$0")"
for arch in x86_64 aarch64; do
    clang --target=$arch-pc-windows-msvc -O1 -fno-stack-protector -c tls.c -o /tmp/tls_$arch.obj
    lld-link /dll /nodefaultlib /entry:DllMain /out:../tls_$arch.dll /noimplib /Brepro /tmp/tls_$arch.obj
done
