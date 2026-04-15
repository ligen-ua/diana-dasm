#!/bin/sh -e

buildDir=cmake-release
mkdir -p "${buildDir}"
cd "${buildDir}"

ctest --verbose 
