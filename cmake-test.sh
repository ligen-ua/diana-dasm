#!/bin/sh -e

buildDir=cmake-release
mkdir -p "${buildDir}"
cd "${buildDir}"

ctest --verbose

cd ..
if python3 -m pytest --version >/dev/null 2>&1; then
    python3 -m pytest tests/cli -q --orthia "${buildDir}/src/orthia/orthia_disasm_ui/orthia_disasm_ui"
else
    echo "command-line tests skipped: python3 or pytest not found (pip install -r tests/cli/requirements.txt)"
fi
