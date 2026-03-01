#!/bin/bash
cd "$(dirname "$0")"

echo "编译 xlang 词法分析工具..."
gcc -o release/xllex main.c -Ilib -O2

if [ $? -ne 0 ]; then
	echo "错误: 编译失败"
	exit 1
fi

echo "完成! 输出: release/xllex"
