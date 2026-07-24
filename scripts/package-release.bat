@echo off
setlocal EnableDelayedExpansion

echo [1/3] Configure Release...
cmake --preset release || exit /b 1

echo [2/3] Build Release...
cmake --build build/release --config Release || exit /b 1

echo [3/3] Package with CPack...
pushd build\release
cpack -G ZIP -C Release
if errorlevel 1 (
    popd
    exit /b 1
)
for %%f in (*.zip) do (
    popd
    echo Release package created: build\release\%%~nxf
    goto :EOF
)
popd
echo Release package created.
