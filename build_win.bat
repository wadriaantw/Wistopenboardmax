@echo off
setlocal

REM === Configure paths ===
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set VS_VCVARS="C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
set OB_DIR=C:\openboard-fork
set TP_DIR=C:\OpenBoard-ThirdParty

REM === Activate MSVC env ===
call %VS_VCVARS%
if errorlevel 1 goto :fail

REM === Add Qt to PATH ===
set PATH=%QT_DIR%\bin;%PATH%

REM === Build quazip first ===
echo ============================================================
echo Step 1: Building QuaZip library
echo ============================================================
cd /d %TP_DIR%\quazip
if not exist build mkdir build
cd build
qmake ..\quazip.pro -spec win32-msvc "CONFIG+=release"
if errorlevel 1 goto :fail
nmake release
if errorlevel 1 goto :fail

REM === Build OpenBoard ===
echo ============================================================
echo Step 2: Building OpenBoard
echo ============================================================
cd /d %OB_DIR%
if not exist build mkdir build
cd build
qmake ..\OpenBoard.pro -spec win32-msvc "CONFIG+=release"
if errorlevel 1 goto :fail
nmake release
if errorlevel 1 goto :fail

echo ============================================================
echo BUILD SUCCESS
echo ============================================================
goto :end

:fail
echo ============================================================
echo BUILD FAILED (errorlevel=%errorlevel%)
echo ============================================================
exit /b 1

:end
endlocal
