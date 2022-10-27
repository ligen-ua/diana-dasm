#!/bin/sh -e

buildDir=cmake
mkdir -p "${buildDir}"
cd "${buildDir}"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
