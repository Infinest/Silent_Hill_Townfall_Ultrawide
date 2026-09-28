@echo off
setlocal
rem Build the mod as a proxy for every tested target (see README.md).
rem Failing targets are reported at the end; the exit code is non-zero if
rem any of them failed.
set TARGETS=dxgi winmm dsound dwmapi bcrypt winhttp opengl32 d3d11 d3d12 xinput1_4 mfreadwrite
set FAILED=
for %%t in (%TARGETS%) do (
    echo ========================================
    echo  Building %%t
    echo ========================================
    call "%~dp0build.bat" %%t >nul
    if errorlevel 1 set FAILED=%FAILED% %%t
)
echo.
if defined FAILED (
    echo FAILED targets:%FAILED%
    exit /b 1
)
echo All targets built into %~dp0dist:
dir /b "%~dp0dist\*.dll"
