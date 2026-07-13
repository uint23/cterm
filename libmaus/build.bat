@echo off

if not exist build mkdir build

set "COMPILER=%~1"
if "%COMPILER%"=="" set "COMPILER=gcc"

if /I "%COMPILER%"=="msvc" goto msvc
if /I "%COMPILER%"=="gcc" goto gcc
if /I "%COMPILER%"=="clang" goto clang

echo Usage: build.bat [msvc^|gcc^|clang]
exit /b 1

:: MSVC
:msvc
cl /diagnostics:color /nologo /W3 /O2 /Iinclude /DBACKEND_WIN /c ^
   source/maus.c source/maus_win.c source/maus_font.c source/utils.c
lib /nologo /OUT:build\libmaus_win.lib *.obj
move /Y *.obj build\
goto :eof

:: GCC
:gcc
cc -std=c99 -Wall -Wextra -O2 -Iinclude -DBACKEND_WIN -c ^
   source/maus.c source/maus_win.c source/maus_font.c source/utils.c
move *.o build\
ar rcs build\libmaus_win.a build\maus.o build\maus_win.o build\maus_font.o build\utils.o
goto :eof

:: GCC
:clang
clang -std=c99 -Wall -Wextra -O2 -Iinclude -DBACKEND_WIN -c ^
   source/maus.c source/maus_win.c source/maus_font.c source/utils.c
move *.o build\
ar rcs build\libmaus_win.a build\maus.o build\maus_win.o build\maus_font.o build\utils.o
goto :eof

