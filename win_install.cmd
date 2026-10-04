@echo off
rem Windows counterpart of `make install`: copies the built orthia binaries and docs.
rem usage: win_install.cmd [debug] [x86] [pdb] [dest]
rem dest: argument, else %ORTHIA_INSTALL_DIR%, else C:\orthia
rem examples:
rem   win_install.cmd                         Release amd64 -> C:\orthia
rem   win_install.cmd D:\tools\orthia         Release amd64 -> D:\tools\orthia
rem   win_install.cmd debug pdb               Debug amd64 with .pdb files -> C:\orthia
rem   win_install.cmd x86 "C:\orthia x86"     Release i386 -> C:\orthia x86
rem   set ORTHIA_INSTALL_DIR=D:\orthia        then plain win_install.cmd -> D:\orthia
setlocal
set "ROOT=%~dp0"
set "SELF=%~nx0"

set CONFIG=Release
set ARCH=amd64
set WITH_PDB=
set DEST=

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="debug" (set CONFIG=Debug& goto next)
if /i "%~1"=="x86"   (set ARCH=i386& goto next)
if /i "%~1"=="pdb"   (set WITH_PDB=1& goto next)
if /i "%~1"=="-h"    goto usage
if /i "%~1"=="/?"    goto usage
if defined DEST goto usage
set "DEST=%~1"
:next
shift
goto parse

:parsed
if not defined DEST set "DEST=%ORTHIA_INSTALL_DIR%"
if not defined DEST set "DEST=C:\orthia"
set "SRC=%ROOT%bin\%CONFIG%\%ARCH%"

for %%f in (orthia.dll orthia.exe orthia_disasm.exe) do (
    if not exist "%SRC%\%%f" (
        echo error: %SRC%\%%f not found, build "%CONFIG%" for %ARCH% first
        exit /b 1
    )
)

if not exist "%DEST%\doc" mkdir "%DEST%\doc" || exit /b 1

for %%f in (orthia.dll orthia.exe orthia_disasm.exe) do call :install "%SRC%\%%f" "%DEST%" || exit /b 1
if defined WITH_PDB (
    for %%f in (orthia.pdb orthia_dll.pdb orthia_disasm.pdb) do (
        if exist "%SRC%\%%f" call :install "%SRC%\%%f" "%DEST%" || exit /b 1
    )
)
for %%f in (README.md LICENSE THIRD_PARTY_NOTICES.md docs\orthia.md docs\databases.md) do (
    call :install "%ROOT%%%f" "%DEST%\doc" || exit /b 1
)
exit /b 0

:install
echo -- Installing: %~2\%~nx1
copy /y "%~1" "%~2\" >nul || (echo error: failed to copy %~1& exit /b 1)
exit /b 0

:usage
echo usage: %SELF% [debug] [x86] [pdb] [dest]
echo   debug  install bin\Debug instead of bin\Release
echo   x86    install the i386 build instead of amd64
echo   pdb    also install the .pdb files
echo   dest   install folder, default %%ORTHIA_INSTALL_DIR%% or C:\orthia
exit /b 1
