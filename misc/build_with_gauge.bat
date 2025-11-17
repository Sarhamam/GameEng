@echo off
REM Build script for GameEng with SU(3)×SU(2)×U(1) Gauge Theory Testbed
REM Usage: Run from misc/ directory

set CommonCompilerFlags=-MTd -nologo -GR- -EHa- -Oi -Od -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -DDEV_BUILD=1 -DSLOW_BUILD=1 -FC -Zi
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM Clean old PDB files
del *.pdb 2> NUL

REM Build platform layer (Win32)
echo Building win32_main.exe...
cl %CommonCompilerFlags% -Fmwin32_main.map ..\code\win32_main.cpp /link %CommonLinkerFlags%

REM Build game DLL with gauge theory modules
echo Building game.dll with gauge theory...
cl %CommonCompilerFlags% -Fmgame.map ^
   ..\code\game.cpp ^
   ..\code\gauge_theory.cpp ^
   ..\code\gauge_render.cpp ^
   /LD /link -PDB:game_%random%.pdb -incremental:no /DLL ^
   /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender

popd

echo.
echo Build complete!
echo Run: build\win32_main.exe
