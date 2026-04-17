@echo off
echo.
echo  DistCache Starting services
echo.
echo  [1/2] Launching C++ backend on http://localhost:8080 ...
start "DistCache Backend" cmd /k app.exe

timeout /t 2 /nobreak >nul

echo  [2/2] Launching frontend on http://localhost:3000 ...
start "DistCache Frontend" cmd /k "cd ui && python -m http.server 3000"

timeout /t 3 /nobreak >nul
echo.
echo  Opening browser...
start http://localhost:3000
echo.
echo  Backend : http://localhost:8080
echo  Frontend: http://localhost:3000
echo.
