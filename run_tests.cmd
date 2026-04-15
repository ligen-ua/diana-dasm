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
:log
echo.%~1
echo.%~1 >> "%LOG_FILE%"
goto :eof
