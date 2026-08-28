@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d C:\openboard-fork\build
nmake release
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

REM Keep custom widgets deployed (they can get lost between builds).
REM Copies are VERIFIED afterwards: a silent xcopy failure once left Grid and
REM YouTube missing from the product while the build still said SUCCESS.
set OUT=C:\openboard-fork\build\build\win32\release\product\library\applications
set DEPLOYFAIL=
for %%W in (SciCalcPlus YouTube GeoGebra GeoGebraGeo Formula Grid NumberLine UnitCircle Spinner SciCalc DiceRoller Stopwatch PeriodicTable PhET MolView Stellarium HumanBody WolframAlpha SlopeField RiemannSum TaylorSeries Transformations ScatterFit Timer FractionTiles) do (
    if exist "C:\openboard-fork\resources\library\applications\%%W.wgt" (
        xcopy /Y /E /I /Q "C:\openboard-fork\resources\library\applications\%%W.wgt" "%OUT%\%%W.wgt" >nul 2>&1
        if not exist "%OUT%\%%W.wgt\index.html" (
            echo WIDGET DEPLOY FAILED: %%W.wgt
            set DEPLOYFAIL=1
        )
    )
)
if defined DEPLOYFAIL (echo BUILD FAILED: widget deploy incomplete & exit /b 1)

REM Keep the app-wide stylesheets deployed too (same silent-loss risk).
copy /Y "C:\openboard-fork\resources\etc\OpenBoard.css" "C:\openboard-fork\build\build\win32\release\product\etc\" >nul
copy /Y "C:\openboard-fork\resources\etc\OpenBoard-dark.css" "C:\openboard-fork\build\build\win32\release\product\etc\" >nul

echo BUILD SUCCESS
endlocal
