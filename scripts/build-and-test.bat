@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0\.."

echo [1/5] Configure Debug...
cmake --preset debug || exit /b 1

echo [2/5] Build Debug...
cmake --build build/debug --config Debug || exit /b 1

echo [3/5] Smoke test (2s auto-close)...
build\debug\bin\MingCKeyOverlay.exe --smoke-test || exit /b 1

echo [4/5] Configure Release...
cmake --preset release || exit /b 1

echo [5/5] Build Release...
cmake --build build/release --config Release || exit /b 1

echo Build and smoke test passed.
