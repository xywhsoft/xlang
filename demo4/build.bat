@echo off
cd /d %~dp0

echo 编译 xlang 编译器工具...
tcc -o release\xlang.exe main.c -Ilib

if errorlevel 1 (
	echo 错误: 编译失败
	pause
	exit /b 1
)

echo 完成! 输出: release\xlang.exe
