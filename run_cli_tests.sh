#!/bin/sh -e
# Command-line (black-box) tests for orthia. Extra arguments go to pytest,
# e.g. ./run_cli_tests.sh -k expressions   or   ./run_cli_tests.sh --orthia path/to/orthia
python3 -m pytest "$(dirname "$0")/tests/cli" "$@"
