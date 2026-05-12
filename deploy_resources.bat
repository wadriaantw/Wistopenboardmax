@echo off
setlocal
set SRC=C:\openboard-fork\resources
set TP=C:\OpenBoard-ThirdParty
set OUT=C:\openboard-fork\build\build\win32\release\product

echo Copying library...
xcopy /Y /E /I /Q "%SRC%\library" "%OUT%\library" >nul

echo Copying customizations...
xcopy /Y /E /I /Q "%SRC%\customizations" "%OUT%\customizations" >nul

echo Copying etc...
xcopy /Y /E /I /Q "%SRC%\etc" "%OUT%\etc" >nul

echo Copying fonts...
xcopy /Y /E /I /Q "%SRC%\fonts" "%OUT%\fonts" >nul

echo Copying startupHints...
xcopy /Y /E /I /Q "%SRC%\startupHints" "%OUT%\startupHints" >nul

echo Copying ThirdParty interactive widgets...
if exist "%TP%\microsoft\interactive" xcopy /Y /E /I /Q "%TP%\microsoft\interactive" "%OUT%\library\interactive" >nul

echo === RESOURCES DONE ===
dir /S /B "%OUT%" | find /c /v ""
endlocal
