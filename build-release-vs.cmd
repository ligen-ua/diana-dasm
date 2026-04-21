call "%VS140COMNTOOLS%vsvars32.bat"
set WINDBGSDKDIR=C:\Program Files (x86)\Windows Kits\10\Debuggers
set WINDBGSDKDIR_SHARED=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" src\orthia_14_0.sln /Build "Release|x64"
