#!/bin/bash
cd "$(dirname "$0")"

echo "生成词法分析器..."
flex xl_lexer.l

if [ $? -ne 0 ]; then
	echo "错误: flex 生成失败"
	exit 1
fi

echo "完成! 生成文件:"
echo "  - xl_lexer.h      (类型定义)"
echo "  - xl_lexer_impl.h (实现代码, 作为头文件 include)"
