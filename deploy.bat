@echo off
setlocal
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set TP_DIR=C:\OpenBoard-ThirdParty
set OUT=C:\openboard-fork\build\build\win32\release\product
set PATH=%QT_DIR%\bin;%PATH%

echo Running windeployqt...
windeployqt --release --no-translations --compiler-runtime "%OUT%\OpenBoard.exe"
if errorlevel 1 goto :fail

echo Copying Poppler DLLs...
copy /Y "%TP_DIR%\poppler\bin\*.dll" "%OUT%\"
if errorlevel 1 goto :fail

echo Copying OpenSSL DLLs...
copy /Y "%TP_DIR%\openssl\openssl-3.0.15-win64\bin\*.dll" "%OUT%\" 2>nul
if not exist "%OUT%\libssl-3-x64.dll" (
    for /f "delims=" %%f in ('dir /b /s "%TP_DIR%\openssl\openssl-3.0.15-win64\*.dll" 2^>nul') do copy /Y "%%f" "%OUT%\"
)

echo Copying zlib DLL...
for /f "delims=" %%f in ('dir /b /s "%TP_DIR%\zlib\1.2.11\*.dll" 2^>nul') do copy /Y "%%f" "%OUT%\"

echo Copying i18n and resources...
if exist "%OUT%\..\..\..\..\..\resources\i18n" (
    if not exist "%OUT%\i18n" mkdir "%OUT%\i18n"
    xcopy /Y /E "C:\openboard-fork\resources\i18n\*" "%OUT%\i18n\"
)

echo.
echo === DEPLOY DONE ===
echo OpenBoard.exe is at: %OUT%\OpenBoard.exe
dir "%OUT%\*.exe" "%OUT%\*.dll" | find "File(s)"
goto :end

:fail
echo DEPLOY FAILED
exit /b 1

:end
endlocal
