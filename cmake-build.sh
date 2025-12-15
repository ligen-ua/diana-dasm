#!/bin/bash -e

buildDir=cmake
mkdir -p "${buildDir}"
cd "${buildDir}"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
