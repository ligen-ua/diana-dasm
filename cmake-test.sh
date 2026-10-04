#!/bin/sh -e

buildDir=cmake-release
mkdir -p "${buildDir}"
cd "${buildDir}"

ctest --verbose

cd ..
if python3 -m pytest --version >/dev/null 2>&1; then
    junit=tests/cli/_out/junit.xml
    rm -f "${junit}"
    rc=0
    python3 -m pytest tests/cli -q --orthia "${buildDir}/src/orthia/orthia_disasm_ui/orthia" --junitxml="${junit}" || rc=$?
    # known bugs are xfail tests and don't fail the run
    echo "command-line tests: $(python3 tests/cli/junit_summary.py "${junit}")"
    exit ${rc}
else
    echo "command-line tests skipped: python3 or pytest not found (pip install -r tests/cli/requirements.txt)"
fi
