@echo off
setlocal enabledelayedexpansion

:: Save script dir BEFORE any cd, so paths stay correct
set "SCRIPT_DIR=%~dp0"
set "EXE_DIR=%SCRIPT_DIR%bin\Release\amd64"
set "LOG_FILE=%SCRIPT_DIR%test_results.log"

cd /d "%EXE_DIR%" || (
    echo [ERROR] Cannot cd to: %EXE_DIR%
    exit /b 1
)

set "FAILED_COUNT=0"
set "FAILED_LIST="

echo. > "%LOG_FILE%"
call :log "============================================================"
call :log " TEST SUITE STARTED  %DATE% %TIME%"
call :log "============================================================"

call :RunTest "diana_core_tests.exe"
call :RunTest "diana_processor_tests.exe"
call :RunTest "diana_win_test.exe"
call :RunTest "orthia_test.exe"
call :RunCliTests

call :log "============================================================"

if %FAILED_COUNT%==0 goto :AllPassed

call :log " !!! %FAILED_COUNT% TEST(S) FAILED !!!"
call :log " "
call :log " Failed executables:"
for %%T in (%FAILED_LIST%) do call :log "   [X] %%~T"
call :log " "
call :log " See full output above or in: %LOG_FILE%"
call :log "============================================================"
exit /b 1

:AllPassed
call :log " ALL TESTS PASSED"
call :log "============================================================"
exit /b 0

:: -----------------------------------------------------------
:RunTest
set "TEST_EXE=%~1"
call :log "------------------------------------------------------------"
call :log "[ RUN ] %TEST_EXE%"
call :log " "

"%EXE_DIR%\%TEST_EXE%" >> "%LOG_FILE%" 2>&1
set "EC=%ERRORLEVEL%"

if %EC% NEQ 0 goto :RunTest_Failed
call :log "  [PASSED]  %TEST_EXE%"
goto :eof

:RunTest_Failed
call :log "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
call :log "  [FAILED]  %TEST_EXE%  exit code: %EC%"
call :log "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
set /a FAILED_COUNT+=1
set "FAILED_LIST=!FAILED_LIST! "%TEST_EXE%""
goto :eof

:: -----------------------------------------------------------
:RunCliTests
call :log "------------------------------------------------------------"
call :log "[ RUN ] command-line tests (tests\cli)"
call :log " "
python -m pytest --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    call :log "  [SKIPPED] python or pytest not found: pip install -r tests\cli\requirements.txt"
    goto :eof
)
set "JUNIT_FILE=%SCRIPT_DIR%tests\cli\_out\junit.xml"
:: a stale report would give a wrong summary if pytest dies before writing a new one
if exist "%JUNIT_FILE%" del "%JUNIT_FILE%"
python -m pytest "%SCRIPT_DIR%tests\cli" -q --orthia "%EXE_DIR%\orthia.exe" --junitxml="%JUNIT_FILE%" >> "%LOG_FILE%" 2>&1
set "EC=%ERRORLEVEL%"
:: e.g. "70 passed, 10 known bugs (11 tests): B1 B2 ..." -- known bugs are xfail tests and don't fail the run
set "CLI_SUMMARY="
for /f "delims=" %%S in ('python "%SCRIPT_DIR%tests\cli\junit_summary.py" "%JUNIT_FILE%"') do set "CLI_SUMMARY=%%S"
if %EC% NEQ 0 goto :RunCliTests_Failed
call :log "  [PASSED]  command-line tests"
call :log "            !CLI_SUMMARY!"
goto :eof

:RunCliTests_Failed
call :log "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
call :log "  [FAILED]  command-line tests  exit code: %EC%"
call :log "            !CLI_SUMMARY!"
call :log "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
set /a FAILED_COUNT+=1
set "FAILED_LIST=!FAILED_LIST! "tests\cli""
goto :eof

:: -----------------------------------------------------------
:log
echo.%~1
echo.%~1 >> "%LOG_FILE%"
goto :eof
