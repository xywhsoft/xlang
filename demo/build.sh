#!/bin/bash
# xlang 编译脚本 - Linux

# 创建输出目录
mkdir -p release

# 编译 xlang (基础版本)
gcc main.c lib/xrt/xrt.c lib/tcc/libtcc.c -Ilib/tcc -Ilib/tcc/include -ldl -lpthread -lm -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/xlang

echo "Build completed!"
