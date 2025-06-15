@echo off
echo Building Loom Compiler with new CLI system...

echo Compiling with g++...
g++ -std=c++17 -I. -Icompiler -O2 -DNDEBUG ^
    -o loom_new.exe ^
    compiler/main_new.cc ^
    compiler/cli/cli.cc ^
    compiler/cli/commands.cc ^
    compiler/codegen/codegen.cc ^
    compiler/syscalls/syscall_framework.cc ^
    compiler/syscalls/syscall_platform_impl.cc ^
    compiler/parser/*.cc ^
    compiler/lexer/*.cc ^
    compiler/sema/*.cc

if %ERRORLEVEL% EQU 0 (
    echo Build successful! New compiler: loom_new.exe
    echo.
    echo Testing new CLI system:
    echo.
    .\loom_new.exe --help
) else (
    echo Build failed!
    exit /b 1
)
