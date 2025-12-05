@echo off
REM xlang DEMO 构建脚本
REM 需要安装 GCC（如 MinGW-w64）

setlocal EnableDelayedExpansion

echo ========================================
echo xlang DEMO Build Script
echo ========================================
echo.

REM 设置工具路径
set FLEX=..\tools\flex.exe
set LEMON=..\tools\lemon.exe
set LEMPAR=..\tools\lempar.c

REM 检查工具是否存在
if not exist "%FLEX%" (
    echo Error: flex.exe not found at %FLEX%
    echo Please ensure flex.exe is in the tools directory
    pause
    exit /b 1
)

if not exist "%LEMON%" (
    echo Error: lemon.exe not found at %LEMON%
    echo Please ensure lemon.exe is in the tools directory
    pause
    exit /b 1
)

REM 步骤 1: 使用 flex 生成词法分析器
echo [1/4] Generating lexer with flex...
%FLEX% xl.l
if errorlevel 1 (
    echo Error: flex failed
    pause
    exit /b 1
)
echo       Lexer generated: xl_lex.c, xl_lex.h

REM 步骤 2: 使用 lemon 生成语法分析器
echo [2/4] Generating parser with lemon...
copy "%LEMPAR%" lempar.c >nul 2>&1
%LEMON% -s xl.y
REM lemon returns non-zero on conflicts, but still generates parser
if not exist xl.c (
    echo Error: lemon failed to generate parser
    pause
    exit /b 1
)
REM lemon 生成 xl.c 和 xl.h，重命名为 xl_parser.c 和 xl_parser.h
if exist xl.c (
    copy /y xl.c xl_parser.c >nul
    del xl.c
)
if exist xl.h (
    copy /y xl.h xl_parser.h >nul
    del xl.h
)
echo       Parser generated: xl_parser.c, xl_parser.h

REM 步骤 3: 编译
echo [3/4] Compiling xlang...

REM 检查是否有 gcc
where gcc >nul 2>&1
if errorlevel 1 (
    echo Error: gcc not found in PATH
    echo Please install MinGW-w64 or add gcc to PATH
    pause
    exit /b 1
)

REM 编译所有源文件
gcc -Wall -Wno-unused-function -Wno-unused-variable ^
    -o xlang.exe ^
    xl_main.c ^
    xl_exec.c ^
    xl_run.c ^
    xl_lex.c ^
    xl_parser.c ^
    -lm

if errorlevel 1 (
    echo Error: Compilation failed
    pause
    exit /b 1
)
echo       Executable created: xlang.exe

REM 步骤 4: 测试
echo [4/4] Testing...
if exist test_hello.xl (
    echo.
    echo --- Running test_hello.xl ---
    xlang.exe test_hello.xl
) else (
    echo       No test file found, skipping test
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Usage:
echo   xlang.exe script.xl     - Run a script file
echo   xlang.exe -e "code"     - Execute code directly  
echo   xlang.exe -i            - Interactive mode (REPL)
echo   xlang.exe -h            - Show help
echo.

pause
