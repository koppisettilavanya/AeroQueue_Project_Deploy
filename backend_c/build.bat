@echo off
echo ==========================================
echo Compiling AeroQueue C Backend...
echo ==========================================
gcc aeroqueue.c -o aeroqueue_server.exe -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    exit /b %errorlevel%
) else (
    echo [SUCCESS] Compiled successfully to aeroqueue_server.exe
)
