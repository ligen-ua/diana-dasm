#!/bin/bash -e

buildType=Release
for arg in "$@"; do
    case "$arg" in
        --debug)   buildType=Debug ;;
        --release) buildType=Release ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

buildDir="cmake-$(echo "$buildType" | tr '[:upper:]' '[:lower:]')"
mkdir -p "${buildDir}"
cd "${buildDir}"

cmake -DCMAKE_BUILD_TYPE="${buildType}" ..
cmake --build .
