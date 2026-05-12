@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d C:\openboard-fork\build
nmake release
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

REM Keep custom widgets deployed (they can get lost between builds).
set OUT=C:\openboard-fork\build\build\win32\release\product\library\applications
for %%W in (YouTube GeoGebra GeoGebraGeo Formula Grid NumberLine UnitCircle Spinner SciCalc DiceRoller Stopwatch PeriodicTable PhET MolView Stellarium HumanBody WolframAlpha) do (
    if exist "C:\openboard-fork\resources\library\applications\%%W.wgt" (
        xcopy /Y /E /I /Q "C:\openboard-fork\resources\library\applications\%%W.wgt" "%OUT%\%%W.wgt" >nul
    )
)

echo BUILD SUCCESS
endlocal
