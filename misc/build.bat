@echo off
REM Dont ignore 4505,4189 for non dev builds

set CommonCompilerFlags=-MTd -nologo -GR- -EHa- -Oi -Od -WX -W4 -wd4201 -wd4100 -wd 4189 -wd4505 -DDEV_BUILD=1 -DSLOW_BUILD=1 -FC -Zi
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
del *.pdb 2> NUL
cl  %CommonCompilerFlags% -Fmwin32_main.map ..\code\win32_main.cpp /link %CommonLinkerFlags%
cl  %CommonCompilerFlags% -Fmgame.map ..\code\game.cpp /LD /link  -PDB:game_%random%.pdb -incremental:no /DLL /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
popd