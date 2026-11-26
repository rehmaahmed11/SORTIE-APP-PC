@echo off
setlocal EnableExtensions
title AMS Sortie - one click Windows build
cd /d "%~dp0"
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "TC=%ROOT%\_toolchain"
set "BLD=%ROOT%\_build"
set "OUT=%ROOT%\Output"
set "DIST=%ROOT%\dist"
set "QTVER=6.7.3"
set "CMAKEVER=3.29.6"
set "PYVER=3.12.7"

echo ==========================================================
echo   AMS Sortie - one click builder (Windows 10/11 64-bit)
echo   by Rehman Ahmed  -  +92 306 0706989
echo ----------------------------------------------------------
echo   Already installed tools are detected and skipped.
echo   Missing tools are downloaded into: %TC%
echo   App goes to:      %OUT%\SortieApp.exe
echo   Installers go to: %DIST%  (MSI / setup.exe, when available)
echo ==========================================================
echo.

if not exist "%ROOT%\CMakeLists.txt" (
    echo ERROR: keep this file next to CMakeLists.txt
    goto fail
)
if not exist "%TC%" mkdir "%TC%"
if not exist "%DIST%" mkdir "%DIST%"

rem ---------------------------------------------------------- 1. CMake
echo [1/5] Checking CMake...
set "CMAKE="
if exist "%TC%\cmake\bin\cmake.exe" set "CMAKE=%TC%\cmake\bin\cmake.exe"
if not defined CMAKE (
    where cmake >nul 2>&1
    if not errorlevel 1 set "CMAKE=cmake"
)
if defined CMAKE (
    echo       found - skipping install.
    goto cmake_done
)
echo       not found - downloading CMake %CMAKEVER% ...
curl -L --fail -o "%TC%\cmake.zip" "https://github.com/Kitware/CMake/releases/download/v%CMAKEVER%/cmake-%CMAKEVER%-windows-x86_64.zip"
if errorlevel 1 goto fail
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Force -LiteralPath '%TC%\cmake.zip' -DestinationPath '%TC%'"
if errorlevel 1 goto fail
if exist "%TC%\cmake" rmdir /s /q "%TC%\cmake"
for /d %%D in ("%TC%\cmake-*-windows-x86_64") do move "%%D" "%TC%\cmake" >nul
del /q "%TC%\cmake.zip" 2>nul
if not exist "%TC%\cmake\bin\cmake.exe" goto fail
set "CMAKE=%TC%\cmake\bin\cmake.exe"
:cmake_done

rem ---------------------------------------------------------- 2. Qt 6 + MinGW
echo [2/5] Checking Qt 6 and the MinGW compiler...
call :detect
if defined QTDIR if defined MINGW (
    echo       found - skipping install.
    goto qt_done
)

call :ensure_python
if errorlevel 1 goto fail

echo       installing aqtinstall ...
"%PY%" -m pip install --upgrade pip aqtinstall
if errorlevel 1 goto fail

if not defined QTDIR (
    echo       downloading Qt %QTVER% ^(MinGW 64-bit^) - a big download, please wait ...
    "%PY%" -m aqt install-qt windows desktop %QTVER% win64_mingw -O "%TC%\Qt"
    if errorlevel 1 goto fail
)
if not defined MINGW (
    echo       downloading the MinGW compiler ...
    "%PY%" -m aqt install-tool windows desktop tools_mingw1120 qt.tools.win64_mingw1120 -O "%TC%\Qt"
    if errorlevel 1 (
        echo       retrying with MinGW 13.1 ...
        "%PY%" -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O "%TC%\Qt"
        if errorlevel 1 goto fail
    )
)
call :detect
if not defined QTDIR (
    echo ERROR: Qt finished installing but the Qt folder was not found.
    goto fail
)
if not defined MINGW (
    echo ERROR: the compiler finished installing but was not found.
    goto fail
)
:qt_done
echo       Qt:    %QTDIR%
echo       MinGW: %MINGW%

rem ---------------------------------------------------------- 3. Compile
echo [3/5] Compiling...
set "PATH=%MINGW%;%QTDIR%\bin;%PATH%"
if exist "%BLD%" rmdir /s /q "%BLD%"
"%CMAKE%" -S "%ROOT%" -B "%BLD%" -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%MINGW%\mingw32-make.exe"
if errorlevel 1 goto fail
"%CMAKE%" --build "%BLD%" --config Release -j 4
if errorlevel 1 goto fail
if not exist "%BLD%\SortieApp.exe" (
    echo ERROR: SortieApp.exe was not produced.
    goto fail
)

rem ---------------------------------------------------------- 4. Package
echo [4/5] Packaging into Output folder...
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"
copy /y "%BLD%\SortieApp.exe" "%OUT%\" >nul
"%QTDIR%\bin\windeployqt.exe" --release --no-translations --compiler-runtime --dir "%OUT%" "%OUT%\SortieApp.exe"
if errorlevel 1 goto fail
copy /y "%ROOT%\README.md" "%OUT%\" >nul 2>&1
copy /y "%ROOT%\LICENSE" "%OUT%\" >nul 2>&1
if not exist "%OUT%\platforms\qwindows.dll" (
    echo ERROR: windeployqt did not copy the Windows platform plugin.
    goto fail
)

rem ---------------------------------------------------------- 5. Installers
echo [5/5] Building installers (optional)...
set "PACKAGED="
if exist "%ProgramFiles(x86)%\NSIS\makensis.exe" set "PATH=%ProgramFiles(x86)%\NSIS;%PATH%"
where makensis >nul 2>&1
if errorlevel 1 (
    echo       NSIS not found - skipping setup.exe
) else (
    "%CMAKE%" --build "%BLD%" --target package --config Release -G "MinGW Makefiles" >nul 2>&1
    where wix >nul 2>&1
    if not errorlevel 1 (
        echo       building MSI with WiX ...
        cpack --config "%BLD%\CPackConfig.cmake" -G WIX -B "%DIST%" >nul
        if not errorlevel 1 set "PACKAGED=1"
    )
    echo       building setup.exe with NSIS ...
    cpack --config "%BLD%\CPackConfig.cmake" -G NSIS -B "%DIST%" >nul
    if not errorlevel 1 set "PACKAGED=1"
)
if not defined PACKAGED (
    echo       no installer toolchain found - only the portable Output folder was built.
    echo       The GitHub Action in .github/workflows builds the MSI automatically.
)

echo.
echo ==========================================================
echo   SUCCESS
echo   App:        %OUT%\SortieApp.exe
If exist "%DIST%" echo   Installers: %DIST%
echo ==========================================================
start "" "%OUT%"
pause
exit /b 0

rem ---------------------------------------------------------- helpers
:detect
set "QTDIR="
set "MINGW="
for /d %%D in ("%TC%\Qt\6.*") do if exist "%%D\mingw_64\bin\Qt6Core.dll" set "QTDIR=%%D\mingw_64"
if not defined QTDIR for /d %%D in ("C:\Qt\6.*") do if exist "%%D\mingw_64\bin\Qt6Core.dll" set "QTDIR=%%D\mingw_64"
for /d %%D in ("%TC%\Qt\Tools\mingw*_64") do if exist "%%D\bin\mingw32-make.exe" set "MINGW=%%D\bin"
if not defined MINGW for /d %%D in ("C:\Qt\Tools\mingw*_64") do if exist "%%D\bin\mingw32-make.exe" set "MINGW=%%D\bin"
exit /b 0

:ensure_python
set "PY="
if exist "%TC%\python\python.exe" (
    set "PY=%TC%\python\python.exe"
    exit /b 0
)
python -c "import sys; sys.exit(0 if sys.version_info>=(3,9) else 1)" >nul 2>&1
if not errorlevel 1 (
    set "PY=python"
    exit /b 0
)
echo       Python 3.9+ not found - installing Python %PYVER% locally ...
curl -L --fail -o "%TC%\python_setup.exe" "https://www.python.org/ftp/python/%PYVER%/python-%PYVER%-amd64.exe"
if errorlevel 1 exit /b 1
start /wait "" "%TC%\python_setup.exe" /quiet InstallAllUsers=0 TargetDir="%TC%\python" PrependPath=0 Include_launcher=0 Include_test=0 Include_doc=0 Shortcuts=0 AssociateFiles=0
del /q "%TC%\python_setup.exe" 2>nul
if not exist "%TC%\python\python.exe" exit /b 1
set "PY=%TC%\python\python.exe"
exit /b 0

:fail
echo.
echo ==========================================================
echo   BUILD FAILED - scroll up to the first error.
echo   Common fixes: check the internet connection and run again;
echo   finished steps are skipped automatically.
echo ==========================================================
pause
exit /b 1
