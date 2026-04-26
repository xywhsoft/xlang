# xlang v4 Demo - 阶段一：词法分析器实现规范

## 1. 概述

**目标**：使用 Flex 实现可重入的词法分析器，支持 xlang v4 一阶段语法列表中定义的所有 Token 类型。

**验收标准**：
- 命令行工具能够解析 xlang 源文件并输出 Token 列表
- 通过所有测试用例
- 词法分析器采用微内核形式，易于扩展

## 2. 技术栈

- **Flex**：demo4/src/lexer/flex.exe
- **xrt**：demo4/lib/xrt/xrt.h (单头文件版本)
- **输出目录**：demo4/src/lexer/

## 3. 目录结构

按照单一 .c 文件 + 多个 .h 文件的开发习惯：

```
demo4/
├── main.c                      # 主入口（唯一的 .c 文件）
├── build.bat                   # Windows 构建脚本
├── build.sh                    # Linux 构建脚本
├── lib/
│   ├── xrt/xrt.h               # xrt 单头文件
│   ├── tcc/                    # TCC 源码
│   └── libtcc.h                # TCC API
├── src/
│   └── lexer/
│       ├── xl_lexer.l          # Flex 源文件
│       ├── xl_lexer.h          # Flex 生成的类型定义
│       ├── xl_lexer_impl.h     # Flex 生成的实现代码（作为头文件 include）
│       ├── xl_lexer_api.h      # Lexer 高级 API 封装
│       ├── build.bat           # Flex 编译脚本（Windows）
│       ├── build.sh            # Flex 编译脚本（Linux）
│       └── flex.exe            # Flex 工具
├── test/
│   └── lexer/
│       └── cases/              # 测试用例
│           ├── literals.xl     # 字面量测试
│           ├── operators.xl    # 运算符测试
│           └── keywords.xl     # 关键字测试
├── tools/
│   ├── lemon.exe
│   └── lempar.c
└── release/
    └── xllex.exe               # 词法分析工具
```

**Flex 生成说明**：
- Flex 生成 `xl_lexer_impl.h`（实现）和 `xl_lexer.h`（类型定义）
- 两个文件都作为头文件在 main.c 中 include
- 保持单一 .c 文件的编译模式

**main.c 结构**：
```c
// main.c - xlang 编译器主入口

#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"

#include "src/lexer/xl_lexer.h"
#include "src/lexer/xl_lexer_impl.h"
#include "src/lexer/xl_lexer_api.h"

int main(int argc, char** argv)
{
	// 主程序逻辑
}
```

## 4. Token 类型定义

### 4.1 Token 类别

所有 Token 常量使用 `XL_` 前缀以避免与 Lemon 语法解析器的关键字冲突：

```c
// Token 类型枚举（在 xl_lexer.l 中定义，由 Flex 生成到 xl_lexer_impl.h）
typedef enum XL_TokenType {
    // === 特殊 Token ===
    XL_TOK_EOF = 0,         // 文件结束
    XL_TOK_ERROR,           // 词法错误
    XL_TOK_NEWLINE,         // 换行（语句分隔）
    
    // === 字面量 ===
    XL_TOK_IDENT,           // 标识符
    XL_TOK_INT,             // 整数（十进制）
    XL_TOK_INT_HEX,         // 整数（十六进制 0x / &H）
    XL_TOK_INT_OCT,         // 整数（八进制 &O）
    XL_TOK_INT_BIN,         // 整数（二进制 0b / &B）
    XL_TOK_FLOAT,           // 浮点数
    XL_TOK_STRING,          // 字符串 "..."
    XL_TOK_STRING_RAW,      // 原始字符串 '...'
    XL_TOK_STRING_MULTI,    // 多行字符串 """..."""
    XL_TOK_STRING_MULTI_RAW,// 多行原始字符串 '''...'''
    XL_TOK_TIME,            // 时间字面量 #...#
    
    // === 内置类型关键字 ===
    XL_KW_NULL,             // null（内置类型）
    XL_KW_FUNCTION,         // function（内置类型）
    
    // === 布尔关键字 ===
    XL_KW_TRUE, XL_KW_FALSE,
    
    // === 类型关键字（注册类型） ===
    XL_KW_BOOL,
    XL_KW_I8, XL_KW_U8, XL_KW_I16, XL_KW_U16, 
    XL_KW_I32, XL_KW_U32, XL_KW_I64, XL_KW_U64,
    XL_KW_INT,              // i64 别名
    XL_KW_F32, XL_KW_F64,
    XL_KW_FLOAT,            // f64 别名
    XL_KW_STRING, XL_KW_TIME, XL_KW_POINT,
    XL_KW_ARRAY, XL_KW_LIST, XL_KW_SET, XL_KW_DICT,
    XL_KW_VAR, XL_KW_AUTO, XL_KW_TYPE, XL_KW_RANGE,
    
    // === 控制流关键字 ===
    XL_KW_IF, XL_KW_THEN, XL_KW_ELSE, XL_KW_ELSEIF, XL_KW_END,
    XL_KW_FOR, XL_KW_IN, XL_KW_TO, XL_KW_STEP,
    XL_KW_WHILE, XL_KW_DO,
    XL_KW_BREAK, XL_KW_CONTINUE, XL_KW_RETURN,
    
    // === 函数关键字 ===
    XL_KW_FUNC, XL_KW_ARG, XL_KW_AS, XL_KW_CONST,
    
    // === 逻辑运算符关键字 ===
    XL_KW_AND, XL_KW_OR, XL_KW_NOT,
    
    // === 比较运算符 ===
    XL_OP_GT, XL_OP_LT, XL_OP_GE, XL_OP_LE, XL_OP_NE, XL_OP_EQ, XL_OP_APPROX,
    
    // === 算术运算符 ===
    XL_OP_PLUS, XL_OP_MINUS, XL_OP_STAR, XL_OP_SLASH, XL_OP_PERCENT, XL_OP_POWER,
    
    // === 位运算符 ===
    XL_OP_AMP, XL_OP_PIPE, XL_OP_BANG, XL_OP_CARET, XL_OP_SHR, XL_OP_SHL,
    
    // === 赋值运算符 ===
    XL_OP_ASSIGN,
    XL_OP_PLUS_ASSIGN, XL_OP_MINUS_ASSIGN, XL_OP_STAR_ASSIGN,
    XL_OP_SLASH_ASSIGN, XL_OP_PERCENT_ASSIGN, XL_OP_POWER_ASSIGN,
    XL_OP_INC, XL_OP_DEC,
    
    // === 特殊运算符 ===
    XL_OP_QUESTION, XL_OP_COLON,
    XL_OP_NULL_COALESCE,    // ??
    XL_OP_NULL_ASSIGN,      // ?=
    XL_OP_DOT, XL_OP_SAFE_DOT,    // . ?.
    XL_OP_SAFE_BRACKET,     // ?[
    XL_OP_ELLIPSIS,         // ...
    XL_OP_ELLIPSIS_DICT,    // ***
    XL_OP_RANGE,            // ..
    
    // === 分隔符 ===
    XL_TOK_LPAREN, XL_TOK_RPAREN,     // ( )
    XL_TOK_LBRACKET, XL_TOK_RBRACKET, // [ ]
    XL_TOK_LBRACE, XL_TOK_RBRACE,     // { }
    XL_TOK_COMMA, XL_TOK_SEMICOLON,   // , ;
    
    XL_TOK_COUNT            // Token 类型数量
} XL_TokenType;
```

### 4.2 Token 结构

```c
// Token 结构（在 xl_lexer.l 中定义）
typedef struct XL_Token {
    XL_TokenType type;      // Token 类型
    char* text;             // Token 文本（源码中的原始文本）
    size_t length;          // 文本长度
    int line;               // 行号（从 1 开始）
    int column;             // 列号（从 1 开始）
    
    // 字面量值（按类型使用）
    union {
        int64_t iVal;       // 整数值
        double fVal;        // 浮点值
        char* sVal;         // 字符串值（已处理转义）
    };
} XL_Token;
```

## 5. Lexer API 设计 (xl_lexer_api.h)

### 5.1 高级 API 封装

```c
// Lexer 句柄
typedef struct XL_Lexer {
    xlscan_t scanner;       // Flex scanner
    XL_LexerExtra extra;    // 额外数据
    const char* source;     // 源码
    size_t sourceLen;       // 源码长度
} XL_Lexer;

// 创建 Lexer（从字符串）
XL_Lexer* xlLexerCreate(const char* source, size_t length, const char* filename);

// 创建 Lexer（从文件）
XL_Lexer* xlLexerCreateFromFile(const char* filepath);

// 销毁 Lexer
void xlLexerDestroy(XL_Lexer* lexer);

// 获取下一个 Token
int xlLexerNextToken(XL_Lexer* lexer, XL_Token* token);

// 获取当前位置信息
int xlLexerGetLine(XL_Lexer* lexer);
int xlLexerGetColumn(XL_Lexer* lexer);

// 获取错误信息
const char* xlLexerGetError(XL_Lexer* lexer);
int xlLexerHasError(XL_Lexer* lexer);
```

### 5.2 Token 辅助函数

```c
// 获取 Token 类型名称（在 xl_lexer_impl.h 中由 Flex 生成）
const char* xlTokenTypeName(XL_TokenType type);

// 打印 Token（调试用）
void xlTokenPrint(XL_Token* token);

// 打印所有 Token
void xlLexerDumpTokens(XL_Lexer* lexer);
```

## 6. Flex 配置 (xl_lexer.l)

### 6.1 Flex 选项

```flex
%option reentrant
%option noyywrap
%option yylineno
%option extra-type="XL_LexerExtra*"
%option prefix="xl"
%option outfile="xl_lexer_impl.h"
%option header-file="xl_lexer.h"
```

### 6.2 生成命令

```batch
# Windows
cd demo4/src/lexer
flex.exe xl_lexer.l

# Linux
cd demo4/src/lexer
flex xl_lexer.l
```

## 7. 命令行工具 (xllex)

```
用法: xllex [选项] <文件>

选项:
  -o <文件>     输出到文件（默认标准输出）
  -j            输出 JSON 格式
  -v            显示详细信息
  -h            显示帮助

示例:
  xllex test.xl           扫描 test.xl 并输出 Token 列表
  xllex -j test.xl        输出 JSON 格式的 Token 列表
```

## 8. 构建脚本

### 8.1 build.bat (Windows)

```batch
@echo off
cd /d %~dp0

echo 编译 xlang 词法分析工具...
tcc -o release\xllex.exe main.c -Ilib

echo 完成!
```

### 8.2 build.sh (Linux)

```bash
#!/bin/bash
cd "$(dirname "$0")"

echo "编译 xlang 词法分析工具..."
gcc -o release/xllex main.c -Ilib -O2

echo "完成!"
```

## 9. 测试用例

### 9.1 字面量测试 (literals.xl)

```xlang
// 整数
123
-456
1_000_000
0xFF
&hABCD
&o777
0b1010
&B1111

// 浮点数
3.14
-2.5
1.0e10
2.5E-3

// 字符串
"hello"
"hello\nworld"
'raw string'
"""
多行
字符串
"""

// 时间
#2000-01-01#
#12:00:00#
#2000-01-01 12:00:00#
```

### 9.2 运算符测试 (operators.xl)

```xlang
+ - * / % **
> < >= <= != == ~=
& | ! ^ >> <<
= += -= *= /= %= **=
++ --
? : ?? ?= . ?. ?[
... *** ..
( ) [ ] { }
, ;
```

### 9.3 关键字测试 (keywords.xl)

```xlang
null function
true false
bool int float string time point
array list set dict
var auto type range
if then else elseif end
for in to step while do
break continue return
func arg as const
and or not
```

## 10. 验收检查清单

- [x] Flex 生成可重入词法分析器代码
- [x] Flex 输出为 .h 文件（xl_lexer_impl.h）
- [x] 所有 Token 常量使用 XL_ 前缀
- [x] 支持所有 v4 一阶段 Token 类型
- [x] 支持中文标识符
- [x] 支持数字下划线分隔符
- [x] 实现 Lexer 高级 API 封装
- [x] 命令行工具能正常运行
- [x] 所有测试用例通过
