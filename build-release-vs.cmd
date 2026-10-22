@echo off
setlocal

rem Usage: build-release-vs.cmd [Win32^|x86^|x64^|ARM64]
rem Without an argument the native platform of this machine is used.

set PLATFORM=%~1
if not "%PLATFORM%"=="" goto normalize

rem Read the native architecture from the registry: it is correct even when
rem this cmd runs under WOW64 or x64 emulation on an ARM64 device.
for /f "tokens=3" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PROCESSOR_ARCHITECTURE 2^>nul ^| "%SystemRoot%\System32\find.exe" "PROCESSOR_ARCHITECTURE"') do set PLATFORM=%%a
if "%PLATFORM%"=="" if defined PROCESSOR_ARCHITEW6432 set PLATFORM=%PROCESSOR_ARCHITEW6432%
if "%PLATFORM%"=="" set PLATFORM=%PROCESSOR_ARCHITECTURE%

:normalize
if /i "%PLATFORM%"=="AMD64" set PLATFORM=x64
if /i "%PLATFORM%"=="x64"   set PLATFORM=x64
if /i "%PLATFORM%"=="x86"   set PLATFORM=Win32
if /i "%PLATFORM%"=="Win32" set PLATFORM=Win32
if /i "%PLATFORM%"=="ARM64" set PLATFORM=ARM64

if "%PLATFORM%"=="x64" goto build
if "%PLATFORM%"=="Win32" goto build
if "%PLATFORM%"=="ARM64" goto build
echo Unsupported platform "%PLATFORM%". Use one of: Win32, x86, x64, ARM64
exit /b 1

:build
echo Building Release^|%PLATFORM%
set WINDBGSDKDIR=C:\Program Files (x86)\Windows Kits\10\Debuggers
set WINDBGSDKDIR_SHARED=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" src\orthia_14_0.sln /Build "Release|%PLATFORM%"
exit /b %ERRORLEVEL%
