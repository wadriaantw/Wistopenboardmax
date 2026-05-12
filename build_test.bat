@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
cl 2>&1 | findstr Version
where nmake.exe
where jom.exe
