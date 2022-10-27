#!/bin/sh -e

buildDir=cmake
mkdir -p "${buildDir}"
cd "${buildDir}"

ctest --verbose 
