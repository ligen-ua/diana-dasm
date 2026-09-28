@echo off
:: Command-line (black-box) tests for orthia.exe. Extra arguments go to pytest,
:: e.g. run_cli_tests.cmd -k expressions   or   run_cli_tests.cmd --orthia path\to\orthia.exe
python -m pytest "%~dp0tests\cli" %*
