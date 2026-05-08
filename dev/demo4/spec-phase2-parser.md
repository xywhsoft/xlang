# xlang v4 Demo - 阶段二：语法分析器实现规范

## 1. 概述

**目标**：使用 Lemon 实现语法分析器，将 Token 流解析为 AST 语法树，AST 使用 xvalue 格式存储。

**前置条件**：阶段一词法分析器已完成并通过验收。

**验收标准**：
- 命令行工具能够解析 xlang 源文件并输出 AST 结构
- AST 以 xvalue (xrt 库) 格式存储，支持打印和序列化
- 通过所有语法测试用例
- 语法分析器采用微内核形式，与代码生成解耦

## 2. 技术栈

- **Lemon**：demo4/tools/lemon.exe
- **模板文件**：demo4/tools/lempar.c
- **xrt**：demo4/lib/xrt/xrt.h
- **输出目录**：demo4/src/parse/

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
│   ├── xl_common.h             # 通用定义
│   ├── xl_token.h              # Token 定义（阶段一）
│   ├── xl_lexer.h              # 词法分析器（阶段一）
│   ├── xl_error.h              # 错误处理
│   ├── xl_ast.h                # AST 节点定义和操作
│   └── xl_parser.h             # 语法分析器实现
├── test/
│   └── parser/
│       └── cases/              # 测试用例
│           ├── expr.xl         # 表达式测试
│           ├── stmt.xl         # 语句测试
│           └── func.xl         # 函数测试
├── tools/
│   ├── flex.exe
│   ├── lemon.exe
│   └── lempar.c
└── release/
    ├── xllex.exe               # 词法分析工具（阶段一）
    └── xlearse.exe             # 语法分析工具
```

**注意**：Lemon 生成的代码将被整合到 xl_parser.h 中，保持单一 .c 文件的开发模式。

**main.c 结构（阶段二）**：
```c
// main.c - xlang 编译器主入口

#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"

#include "src/xl_common.h"
#include "src/xl_token.h"
#include "src/xl_error.h"
#include "src/xl_lexer.h"
#include "src/xl_ast.h"
#include "src/xl_parser.h"

int main(int argc, char** argv)
{
	// 主程序逻辑
}
```

## 4. AST 节点设计 (xl_ast.h)

### 4.1 AST 节点类型

```c
// AST 节点类型枚举
typedef enum XL_AstType {
	// === 程序结构 ===
	AST_PROGRAM = 1,            // 程序根节点
	AST_BLOCK,                  // 语句块
	
	// === 字面量 ===
	AST_LIT_NULL,               // null
	AST_LIT_BOOL,               // true / false
	AST_LIT_INT,                // 整数
	AST_LIT_FLOAT,              // 浮点数
	AST_LIT_STRING,             // 字符串
	AST_LIT_TIME,               // 时间
	AST_LIT_ARRAY,              // 数组字面量 [...]
	AST_LIT_LIST,               // 列表字面量 list{...}
	AST_LIT_SET,                // 集合字面量 set{...}
	AST_LIT_DICT,               // 字典字面量 {...}
	
	// === 表达式 ===
	AST_IDENT,                  // 标识符
	AST_BINARY,                 // 二元表达式
	AST_UNARY,                  // 一元表达式
	AST_CALL,                   // 函数调用
	AST_INDEX,                  // 索引访问 a[i]
	AST_MEMBER,                 // 成员访问 a.b
	AST_SAFE_MEMBER,            // 安全成员访问 a?.b
	AST_SAFE_INDEX,             // 安全索引访问 a?[i]
	AST_TERNARY,                // 三元表达式 a ? b : c
	AST_NULL_COALESCE,          // 空值合并 a ?? b
	AST_RANGE,                  // 范围表达式 a..b
	AST_TYPE_CONVERT,           // 类型转换 type(expr)
	
	// === 语句 ===
	AST_VAR_DECL,               // 变量声明
	AST_CONST_DECL,             // 常量声明
	AST_ASSIGN,                 // 赋值语句
	AST_COMPOUND_ASSIGN,        // 复合赋值 +=, -= 等
	AST_EXPR_STMT,              // 表达式语句
	AST_IF,                     // if 语句
	AST_FOR_IN,                 // for...in 循环
	AST_FOR_RANGE,              // for i = a to b 循环
	AST_WHILE,                  // while 循环
	AST_DO_WHILE,               // do...while 循环
	AST_BREAK,                  // break
	AST_CONTINUE,               // continue
	AST_RETURN,                 // return
	
	// === 函数 ===
	AST_FUNC_DECL,              // 函数声明
	AST_PARAM,                  // 参数定义
	AST_PARAM_LIST,             // 参数列表
	AST_ARG_LIST,               // 调用参数列表
	
	// === 类型 ===
	AST_TYPE_REF,               // 类型引用
	AST_TYPE_ARRAY,             // 数组类型 array<T>
	AST_TYPE_FUNC,              // 函数类型
	
	AST_COUNT
} XL_AstType;
```

### 4.2 AST 使用 xvalue 存储

每个 AST 节点存储为 xvalue Table (字典)，包含以下标准字段：

```c
// AST 节点结构 (xvalue Table)
// {
//     "type": int,           // AST_xxx 类型常量
//     "line": int,           // 源码行号
//     "column": int,         // 源码列号
//     ... 节点特定字段 ...
// }

// 创建 AST 节点
xvalue xlAstCreate(XL_AstType type, int line, int column);

// 设置节点字段
void xlAstSetInt(xvalue node, const char* key, int64_t value);
void xlAstSetFloat(xvalue node, const char* key, double value);
void xlAstSetString(xvalue node, const char* key, const char* value);
void xlAstSetNode(xvalue node, const char* key, xvalue child);
void xlAstSetArray(xvalue node, const char* key, xvalue array);

// 获取节点字段
int64_t xlAstGetInt(xvalue node, const char* key);
double xlAstGetFloat(xvalue node, const char* key);
const char* xlAstGetString(xvalue node, const char* key);
xvalue xlAstGetNode(xvalue node, const char* key);
xvalue xlAstGetArray(xvalue node, const char* key);

// 获取节点类型
XL_AstType xlAstGetType(xvalue node);
```

### 4.3 各节点类型的字段定义

```c
// AST_PROGRAM: 程序根节点
// {
//     "type": AST_PROGRAM,
//     "statements": [stmt1, stmt2, ...]  // 语句数组
// }

// AST_VAR_DECL: 变量声明
// {
//     "type": AST_VAR_DECL,
//     "varType": "int" | "var" | ...,     // 类型名
//     "name": "varName",                   // 变量名
//     "init": expr | null                  // 初始化表达式（可选）
// }

// AST_FUNC_DECL: 函数声明
// {
//     "type": AST_FUNC_DECL,
//     "name": "funcName",                  // 函数名
//     "params": [param1, param2, ...],     // 参数列表
//     "returnType": "int" | null,          // 返回类型（可选）
//     "body": [stmt1, stmt2, ...],         // 函数体
//     "isVariadic": false,                 // 是否可变参数 (...)
//     "isVariadicDict": false              // 是否字典形式 (***)
// }

// AST_PARAM: 参数定义
// {
//     "type": AST_PARAM,
//     "paramType": "int" | "var" | ...,   // 参数类型
//     "name": "paramName",                 // 参数名
//     "default": expr | null               // 默认值（可选）
// }

// AST_IF: if 语句
// {
//     "type": AST_IF,
//     "condition": expr,                   // 条件表达式
//     "then": [stmt1, ...],                // then 分支
//     "elseifs": [                         // elseif 分支列表
//         {"condition": expr, "body": [...]},
//         ...
//     ],
//     "else": [stmt1, ...] | null          // else 分支（可选）
// }

// AST_FOR_IN: for...in 循环
// {
//     "type": AST_FOR_IN,
//     "var": "varName",                    // 循环变量
//     "var2": "keyName" | null,            // 第二变量（用于 k,v in dict）
//     "iter": expr,                        // 迭代对象
//     "body": [stmt1, ...]                 // 循环体
// }

// AST_FOR_RANGE: for i = a to b 循环
// {
//     "type": AST_FOR_RANGE,
//     "var": "i",                          // 循环变量
//     "start": expr,                       // 起始值
//     "end": expr,                         // 结束值
//     "step": expr | null,                 // 步长（可选）
//     "body": [stmt1, ...]                 // 循环体
// }

// AST_BINARY: 二元表达式
// {
//     "type": AST_BINARY,
//     "op": "+" | "-" | "*" | ...,        // 运算符
//     "left": expr,                        // 左操作数
//     "right": expr                        // 右操作数
// }

// AST_CALL: 函数调用
// {
//     "type": AST_CALL,
//     "callee": expr,                      // 被调用对象
//     "args": [arg1, arg2, ...],           // 参数列表
//     "namedArgs": {                       // 命名参数
//         "name1": expr1,
//         "name2": expr2
//     }
// }

// AST_LIT_ARRAY: 数组字面量
// {
//     "type": AST_LIT_ARRAY,
//     "elements": [expr1, expr2, ...]      // 元素列表
// }

// AST_LIT_DICT: 字典字面量
// {
//     "type": AST_LIT_DICT,
//     "pairs": [                           // 键值对列表
//         {"key": expr, "value": expr},
//         ...
//     ]
// }
```

## 5. Parser 接口设计 (xl_parser.h)

### 5.1 Parser 上下文

```c
// Parser 上下文
typedef struct XL_Parser {
	XL_LexerHandle lexer;       // 词法分析器
	xvalue ast;                 // AST 根节点
	xvalue errors;              // 错误列表
	const char* filename;       // 文件名
} XL_Parser;

// 创建 Parser
XL_Parser* xlParserCreate(const char* source, size_t length, const char* filename);

// 从文件创建 Parser
XL_Parser* xlParserCreateFromFile(const char* filepath);

// 销毁 Parser
void xlParserDestroy(XL_Parser* parser);

// 解析源码，返回 AST
xvalue xlParserParse(XL_Parser* parser);

// 获取错误列表
xvalue xlParserGetErrors(XL_Parser* parser);

// 是否有错误
bool xlParserHasError(XL_Parser* parser);
```

### 5.2 AST 打印和调试

```c
// 打印 AST（调试用）
void xlAstPrint(xvalue ast, int indent);

// AST 转 JSON 字符串
char* xlAstToJson(xvalue ast);

// 释放 JSON 字符串
void xlAstFreeJson(char* json);
```

## 6. Lemon 语法文件规范 (xl_grammar.y)

### 6.1 文件结构

```lemon
%include {
#include <stdio.h>
#include <stdlib.h>
#include "xl_ast.h"
#include "xl_parser.h"

// Parser 上下文
typedef struct {
	XL_Parser* parser;
	xvalue currentAst;
} ParseContext;
}

%token_type {XL_Token*}
%extra_argument {ParseContext* ctx}
%token_prefix TOK_

%syntax_error {
	xlParserAddError(ctx->parser, "语法错误", TOKEN);
}

%parse_failure {
	xlParserAddError(ctx->parser, "解析失败", NULL);
}
```

### 6.2 优先级定义

```lemon
// 运算符优先级（从低到高）
%left OR.
%left AND.
%right NOT.
%left EQ NE.
%left LT LE GT GE.
%left PIPE.
%left CARET.
%left AMP.
%left SHL SHR.
%left PLUS MINUS.
%left STAR SLASH PERCENT.
%right POWER.
%right UMINUS UPLUS BANG.
%left DOT SAFE_DOT LBRACKET SAFE_BRACKET.
```

### 6.3 核心语法规则示例

```lemon
// 程序
program ::= stmt_list(L). {
	ctx->currentAst = xlAstCreateProgram(L);
}

// 语句列表
stmt_list(A) ::= . {
	A = xlAstCreateArray();
}
stmt_list(A) ::= stmt_list(L) stmt(S). {
	xvoArrayAppendValue(L, S, TRUE);
	A = L;
}

// 变量声明
stmt(A) ::= type_name(T) IDENT(N) ASSIGN expr(E) NEWLINE. {
	A = xlAstCreateVarDecl(T, N->text, E, N->line, N->column);
}

// 函数声明
stmt(A) ::= FUNC IDENT(N) LPAREN param_list(P) RPAREN func_return(R) NEWLINE stmt_list(B) END NEWLINE. {
	A = xlAstCreateFuncDecl(N->text, P, R, B, N->line, N->column);
}

// if 语句
stmt(A) ::= IF expr(C) THEN NEWLINE stmt_list(T) elseif_list(EI) else_part(E) END NEWLINE. {
	A = xlAstCreateIf(C, T, EI, E, ...);
}

// 表达式
expr(A) ::= expr(L) PLUS expr(R). {
	A = xlAstCreateBinary("+", L, R);
}

// 函数调用
expr(A) ::= expr(C) LPAREN arg_list(ARGS) RPAREN. {
	A = xlAstCreateCall(C, ARGS);
}
```

## 7. 回调接口设计（微内核）

语法分析器采用微内核形式，通过回调函数处理 AST：

```c
// AST 节点回调函数类型
typedef void (*XL_AstCallback)(xvalue node, void* userData);

// 设置 AST 回调
void xlParserSetCallback(XL_Parser* parser, XL_AstCallback callback, void* userData);

// 遍历 AST（调用回调）
void xlAstTraverse(xvalue ast, XL_AstCallback callback, void* userData);
```

## 8. 命令行工具 (xlearse)

```
用法: xlearse [选项] <文件>

选项:
  -o <文件>     输出到文件（默认标准输出）
  -j            输出 JSON 格式
  -p            输出美化的 AST 树形结构
  -t            同时输出 Token 列表
  -v            显示详细信息
  -h            显示帮助

示例:
  xlearse test.xl           解析 test.xl 并输出 AST
  xlearse -j test.xl        输出 JSON 格式的 AST
  xlearse -p test.xl        输出美化的 AST 树形结构
```

## 9. 构建脚本

### 9.1 build.bat (Windows)

```batch
@echo off
cd /d %~dp0

echo 编译 xlang 语法分析工具...
tcc -o release\xlearse.exe main.c -Ilib

echo 完成!
```

### 9.2 build.sh (Linux)

```bash
#!/bin/bash
cd "$(dirname "$0")"

echo "编译 xlang 语法分析工具..."
gcc -o release/xlearse main.c -Ilib -O2

echo "完成!"
```

## 10. 测试用例

### 10.1 表达式测试 (expr.xl)

```xlang
// 算术表达式
1 + 2 * 3
(1 + 2) * 3
2 ** 3
-5
!true

// 比较表达式
1 < 2
a == b
x != null

// 逻辑表达式
a and b
a or b
not a

// 成员访问
obj.prop
obj?.prop
arr[0]
arr?[0]

// 函数调用
print("hello")
add(1, 2)
config(name: "test", value: 123)
```

### 10.2 语句测试 (stmt.xl)

```xlang
// 变量声明
int a = 1
var b = "hello"
auto c = [1, 2, 3]
const PI = 3.14159

// if 语句
if a > 0 then
	print("positive")
elseif a < 0 then
	print("negative")
else
	print("zero")
end

// 循环
for i = 1 to 10
	print(i)
end

for item in arr
	print(item)
end

while x > 0
	x -= 1
end
```

### 10.3 函数测试 (func.xl)

```xlang
// 简单函数
func greet()
	println("Hello!")
end

// 带参数
func add(int a, int b) as int
	return a + b
end

// 默认参数
func greet(string name, string msg = "Hello")
	println(msg + ", " + name)
end

// 可变参数
func sum(...)
	int total = 0
	for v in arg
		total += v
	end
	return total
end
```

## 11. 验收检查清单

- [ ] Lemon 生成语法分析器代码
- [ ] 正确解析所有 v4 一阶段语法
- [ ] AST 以 xvalue 格式存储
- [ ] 支持 xvoPrintValue 打印 AST
- [ ] 支持 AST 转 JSON
- [ ] 正确处理运算符优先级
- [ ] 正确处理语法错误并报告
- [ ] 命令行工具能正常运行
- [ ] 所有测试用例通过
- [ ] 内存无泄漏

## 12. 与阶段三的接口

阶段二输出的 AST 将作为阶段三代码生成的输入。关键接口：

```c
// 阶段三将使用以下函数遍历 AST
void xlAstTraverse(xvalue ast, XL_AstCallback callback, void* userData);

// AST 节点类型判断
XL_AstType xlAstGetType(xvalue node);

// 获取节点字段
int64_t xlAstGetInt(xvalue node, const char* key);
const char* xlAstGetString(xvalue node, const char* key);
xvalue xlAstGetNode(xvalue node, const char* key);
xvalue xlAstGetArray(xvalue node, const char* key);
```
