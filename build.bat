@echo off
echo.
echo  Building DistCache HTTP Server...
g++ -std=c++11 -O2 ^
    -Iinclude ^
    src/main.cpp ^
    src/dist_cache.cpp ^
    src/lru_cache.cpp ^
    src/consistent_hash.cpp ^
    -o app.exe ^
    -lws2_32
if %ERRORLEVEL% == 0 (
    echo.
    echo  [OK] Build successful! Output: app.exe
    echo.
) else (
    echo.
    echo  [FAIL] Build failed. Check errors above.
    echo.
)
pause
