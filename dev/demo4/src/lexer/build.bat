@echo off
cd /d %~dp0

echo 生成词法分析器...
flex.exe xl_lexer.l

if errorlevel 1 (
	echo 错误: flex 生成失败
	pause
	exit /b 1
)

echo 完成! 生成文件:
echo   - xl_lexer.h      (类型定义)
echo   - xl_lexer_impl.h (实现代码, 作为头文件 include)
