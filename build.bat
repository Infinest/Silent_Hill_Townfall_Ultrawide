@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

rem Build the mod as a proxy for a system DLL the game imports statically.
rem   build.bat            -> dxgi.dll (default)
rem   build.bat winmm      -> winmm.dll
rem   build.bat dsound     -> dsound.dll
rem Any system DLL whose export table is fully named (no ordinal-only or
rem forwarded exports) works; the stub list is generated from the real DLL.
set TARGET=%~1
if "%TARGET%"=="" set TARGET=dxgi
if /i "%TARGET:~-4%"==".dll" set TARGET=%TARGET:~0,-4%

set ROOT=%~dp0
set OUT=%ROOT%\dist
set GEN=%OUT%\gen
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%GEN%" mkdir "%GEN%"

set REALDLL=%SystemRoot%\System32\%TARGET%.dll
if not exist "%REALDLL%" (echo Unknown target: %REALDLL% not found & exit /b 1)

python "%ROOT%\tools\gen_proxy.py" "%REALDLL%" "%GEN%\%TARGET%_stubs.cpp" "%GEN%\%TARGET%.def" || exit /b 1

cl /nologo /MT /O2 /GL /W4 /GS- ^
   /I"%ROOT%\src" ^
   /DPROXY_TARGET_DLL=\"%TARGET%.dll\" ^
   /Fe:"%OUT%\%TARGET%.dll" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   "%ROOT%\src\dllmain.cpp" ^
   "%GEN%\%TARGET%_stubs.cpp" ^
   "%ROOT%\src\proxy.cpp" ^
   "%ROOT%\src\uiconstraint.cpp" "%ROOT%\src\log.cpp" "%ROOT%\src\detour.cpp" "%ROOT%\src\hooks.cpp" ^
   /link /nologo /DLL /OPT:REF /OPT:ICF /LTCG /DEF:"%GEN%\%TARGET%.def" user32.lib kernel32.lib || exit /b 1

echo.
echo Built: %OUT%\%TARGET%.dll
echo Install: copy %OUT%\%TARGET%.dll into
echo   N:\SteamLibrary\steamapps\common\Townfall\Townfall\Binaries\Win64\
