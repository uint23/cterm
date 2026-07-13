@echo off

set "COMPILER=%~1"
if "%COMPILER%"=="" set "COMPILER=gcc"

if /I "%COMPILER%"=="msvc" goto msvc
if /I "%COMPILER%"=="gcc" goto gcc
if /I "%COMPILER%"=="clang" goto clang

echo Usage: build.bat [msvc^|gcc^|clang]
exit /b 1

:msvc
cl /diagnostics:color /nologo /W3 /O2 /I..\..\include main.c ..\..\build\libmaus_win.lib user32.lib gdi32.lib /link /OUT:basic-window.exe
echo Build complete
goto :eof

:gcc
cc -std=c99 -Wall -Wextra -g -I../../include main.c ../../build/libmaus_win.a -lgdi32 -luser32 -o basic-window.exe
echo Build complete
goto :eof

:clang
clang -std=c99 -Wall -Wextra -g -I../../include main.c ../../build/libmaus_win.a -lgdi32 -luser32 -o basic-window.exe
echo Build complete
goto :eof

