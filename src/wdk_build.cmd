@echo off
rem check DDK path
if not "%1" == "" goto SET_BASE_DDK_PATH
echo specify base DDKPath (param 1)
goto ERROR

:SET_BASE_DDK_PATH
set BASE_DDK_PATH=%1
set BASE_DDK_FILENAME=%~n1

if not "%2" == "" goto SET_PROJECT_DIR
echo specify project directory (param 2)
goto ERROR

:SET_PROJECT_DIR
SET PROJECT_DIR=%3

if not "%3" == "" goto SET_DDK_CONF_TYPE
echo specify configuration name
goto ERROR

:SET_DDK_CONF_TYPE
SET PROJECT_CONFIGURATION_NAME=%2

if "%2" == "Debug" SET DDK_CONF_TYPE=chk
if "%2" == "Release" SET DDK_CONF_TYPE=free

if "%DDK_CONF_TYPE%" == "" goto ERROR


set DDK_3790_USED=0
if "%BASE_DDK_FILENAME%" == "3790" (
set DDK_3790_USED=1
)

rem defaults
SET PLATFORM_VERSION=32
SET BUILD_PARAM=-cfIE


:NextArg

if "%4" == "" goto StartBuild
if "%4" == "-build" goto SetBuildParam
if "%4" == "x64" goto Platform64

echo Invalid argument: "%4"
goto ERROR


rem ---------------------------------
:Platform64
rem PLATFORM x64
if not "%BASE_DDK_FILENAME%" == "7600.16385" if not "%BASE_DDK_FILENAME%" == "7600" (
	SET PLATFORM_VERSION_TO_PASS= amd64
	goto end64version
)

SET PLATFORM_VERSION_TO_PASS= x64

:end64version

SET PLATFORM_VERSION=64
goto EndCmd;
rem ---------------------------------

rem ---------------------------------
:SetBuildParam

SET BUILD_PARAM=/cfIE

goto EndCmd;
rem ---------------------------------

:EndCmd

shift
goto :NextArg

:StartBuild

echo Platform version: %PLATFORM_VERSION%

echo DDK path is %BASE_DDK_PATH%
call %BASE_DDK_PATH%\bin\setenv.bat %BASE_DDK_PATH% %DDK_CONF_TYPE% wnet %PLATFORM_VERSION_TO_PASS%
echo TARGET_INC_PATH: %TARGET_INC_PATH%
echo WDM_INC_PATH: %WDM_INC_PATH%

SET BUILD_ALLOW_LINKER_WARNINGS=1
cd /d %PROJECT_DIR%
echo del %BASE_DDK_PATH%\build.dat
del "%BASE_DDK_PATH%\build.dat"
@echo build.exe %BUILD_PARAM% %PLATFORM_VERSION_TO_PASS%
build.exe %BUILD_PARAM% %PLATFORM_VERSION_TO_PASS%
goto :EOF
:ERROR
echo FATAL ERROR !