@echo off
subst w: C:\Users\Saar\Desktop\DIYGame
w:
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=W:\misc\;%path%