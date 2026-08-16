@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "OUTPUT=%SCRIPT_DIR%\tig-pkg.exe"
set "SOURCE=%SCRIPT_DIR%\src\main.cpp"

if not defined CXX set "CXX=g++"
set "CXXFLAGS=-std=c++20 -O3 -march=native -Wall -Wextra"
set "LDFLAGS=-lcurl -lws2_32"

echo Compiling: %SOURCE% -^> %OUTPUT%

%CXX% %CXXFLAGS% "%SOURCE%" -o "%OUTPUT%" %LDFLAGS%

if %ERRORLEVEL% equ 0 (
    echo Compilation successful: %OUTPUT%
) else (
    echo Compilation failed.
    exit /b 1
)
