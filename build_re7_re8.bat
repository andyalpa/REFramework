@echo off
REM Build only RE7 and RE8 targets for local testing.
setlocal
cd /d "%~dp0"

echo [1/4] Updating submodules...
git submodule update --init --recursive
if errorlevel 1 (
    echo Submodule update failed.
    exit /b 1
)

echo [2/4] Configuring CMake (RE7 + RE8)...
if not exist build_re7_re8 mkdir build_re7_re8
cd build_re7_re8
cmake .. -G "Visual Studio 17 2022" -A x64 -DDEVELOPER_MODE=ON
if errorlevel 1 (
    echo CMake configure failed.
    cd ..
    exit /b 1
)

echo [3/4] Building RE7...
cmake --build . --config Release --target RE7
if errorlevel 1 (
    echo RE7 build failed.
    cd ..
    exit /b 1
)

echo [4/4] Building RE8...
cmake --build . --config Release --target RE8
if errorlevel 1 (
    echo RE8 build failed.
    cd ..
    exit /b 1
)

cd ..
echo.
echo Build succeeded. Outputs:
echo   RE7: build_re7_re8\bin\RE7\dinput8.dll
echo   RE8: build_re7_re8\bin\RE8\dinput8.dll
exit /b 0
