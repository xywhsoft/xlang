@echo off
cd /d %~dp0

echo 编译 xlang 编译器工具 (GCC 优化)...
gcc -o release\xlang.exe main.c -Ilib -lws2_32 -lIPHLPAPI -O2

if errorlevel 1 (
	echo 错误: 编译失败
	pause
	exit /b 1
)

echo 完成! 输出: release\xlang.exe
