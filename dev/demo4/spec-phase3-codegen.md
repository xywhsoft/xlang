# xlang v4 Demo - 阶段三：类型注册与代码生成实现规范

## 1. 概述

**目标**：实现类型注册系统和 C 代码生成器，将 AST 编译为可执行的 C 代码，通过 TCC 运行或 GCC 编译。

**前置条件**：阶段一词法分析器、阶段二语法分析器已完成并通过验收。

**用户参与要求**：本阶段每个环节都需要用户人工参与和确认，确保对 xlang 有完整的理解和掌控力。

**验收标准**：
- 类型注册系统支持所有 v4 一阶段类型
- 代码生成器能够生成正确的 C 代码
- 生成的代码能够通过 TCC/GCC 编译
- 脚本模式能够直接运行 xlang 程序
- 编译模式能够生成可执行文件
- 通过所有综合测试用例

## 2. 技术栈

- **TCC**：demo4/lib/tcc/ (libtcc.dll)
- **xrt**：demo4/lib/xrt/xrt.h
- **输出目录**：demo4/src/codegen/

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
│   ├── xl_ast.h                # AST 定义（阶段二）
│   ├── xl_parser.h             # 语法分析器（阶段二）
│   ├── xl_type.h               # 类型系统核心
│   ├── xl_type_builtin.h       # 内置类型 (function, null)
│   ├── xl_type_atomic.h        # 原子类型 (bool, int, float, time, point)
│   ├── xl_type_string.h        # 字符串类型
│   ├── xl_type_container.h     # 容器类型 (array, list, set, dict)
│   ├── xl_type_special.h       # 特殊类型 (var, type, range)
│   ├── xl_codegen.h            # 代码生成器核心
│   ├── xl_codegen_expr.h       # 表达式代码生成
│   ├── xl_codegen_stmt.h       # 语句代码生成
│   ├── xl_codegen_func.h       # 函数代码生成
│   ├── xl_runtime.h            # 运行时库
│   └── xl_tcc.h                # TCC 集成
├── test/
│   └── cases/                  # 综合测试用例
│       ├── hello.xl
│       ├── vars.xl
│       ├── control.xl
│       ├── func.xl
│       └── containers.xl
├── tools/
│   ├── flex.exe
│   ├── lemon.exe
│   └── lempar.c
└── release/
    ├── xllex.exe               # 词法分析工具
    ├── xlearse.exe             # 语法分析工具
    ├── xlc.exe                 # xlang 编译器
    ├── xl.exe                  # xlang 解释器
    └── cgen/                   # 生成的 C 代码输出
```

**main.c 结构（完整版）**：
```c
// main.c - xlang 编译器主入口

#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#include "lib/libtcc.h"

// 通用定义
#include "src/xl_common.h"
#include "src/xl_error.h"

// 阶段一：词法分析
#include "src/xl_token.h"
#include "src/xl_lexer.h"

// 阶段二：语法分析
#include "src/xl_ast.h"
#include "src/xl_parser.h"

// 阶段三：类型系统
#include "src/xl_type.h"
#include "src/xl_type_builtin.h"
#include "src/xl_type_atomic.h"
#include "src/xl_type_string.h"
#include "src/xl_type_container.h"
#include "src/xl_type_special.h"

// 阶段三：代码生成
#include "src/xl_codegen.h"
#include "src/xl_codegen_expr.h"
#include "src/xl_codegen_stmt.h"
#include "src/xl_codegen_func.h"

// 运行时和 TCC 集成
#include "src/xl_runtime.h"
#include "src/xl_tcc.h"

int main(int argc, char** argv)
{
	// 主程序逻辑
}
```

**编译命令**：
```batch
:: Windows (TCC 快速编译)
tcc -o release\xl.exe main.c -Ilib -Llib\tcc -ltcc

:: Windows (GCC 优化编译)
gcc -o release\xl.exe main.c -Ilib -Llib\tcc -ltcc -O2
```

## 4. 类型注册系统 (xl_type.h)

### 4.1 设计原则

参考 `xlang类型注册系统设计.txt`：

- xlang 编译器核心认识两种内置类型：`function` 和 `null`
- 所有其他类型都是「注册类型」
- 类型注册发生在编译器初始化阶段
- 每个类型提供代码生成钩子

### 4.2 类型描述符

```c
// 类型 ID 常量
#define XL_TYPE_FUNCTION    0   // 内置类型：函数
#define XL_TYPE_NULL        1   // 内置类型：空值

// 标准类型 ID（100-999）
#define XL_TYPE_BOOL        101
#define XL_TYPE_I8          110
#define XL_TYPE_U8          111
#define XL_TYPE_I16         112
#define XL_TYPE_U16         113
#define XL_TYPE_I32         114
#define XL_TYPE_U32         115
#define XL_TYPE_I64         116
#define XL_TYPE_U64         117
#define XL_TYPE_F32         120
#define XL_TYPE_F64         121
#define XL_TYPE_TIME        122
#define XL_TYPE_POINT       123
#define XL_TYPE_STRING      130
#define XL_TYPE_ARRAY       140
#define XL_TYPE_LIST        141
#define XL_TYPE_SET         142
#define XL_TYPE_DICT        143
#define XL_TYPE_VAR         150
#define XL_TYPE_TYPE        151
#define XL_TYPE_RANGE       152

// 别名
#define XL_TYPE_INT         XL_TYPE_I64
#define XL_TYPE_FLOAT       XL_TYPE_F64

// 类型类别
#define XL_TYPE_CAT_BUILTIN     0   // 内置类型
#define XL_TYPE_CAT_ATOMIC      1   // 原子类型
#define XL_TYPE_CAT_COMPOUND    2   // 复合类型
#define XL_TYPE_CAT_CONTAINER   3   // 容器类型
#define XL_TYPE_CAT_SPECIAL     4   // 特殊类型

// 代码生成上下文
typedef struct XL_CodegenContext {
	struct XL_Compiler* compiler;   // 编译器上下文
	xvalue ast;                     // 当前 AST 节点
	char* buffer;                   // 输出缓冲区
	size_t bufferSize;              // 缓冲区大小
	size_t bufferUsed;              // 已使用大小
	int indentLevel;                // 缩进级别
} XL_CodegenContext;

// 类型描述符
typedef struct XL_TypeDesc {
	int typeId;                     // 类型 ID
	const char* name;               // 类型名称
	const char* cType;              // 对应的 C 类型
	int category;                   // 类型类别
	size_t size;                    // 类型大小（字节）
	
	// 代码生成钩子
	void (*procCodegenDecl)(XL_CodegenContext* ctx, const char* varName, xvalue initExpr);
	void (*procCodegenConvert)(XL_CodegenContext* ctx, xvalue expr, int targetType);
	void (*procCodegenOp)(XL_CodegenContext* ctx, const char* op, xvalue left, xvalue right);
	void (*procCodegenIndex)(XL_CodegenContext* ctx, xvalue container, xvalue index);
	void (*procCodegenCall)(XL_CodegenContext* ctx, xvalue callee, xvalue args);
	
	// 运算符支持表
	void* opTable;                  // 运算符实现表
	
	// 类型转换支持表
	void* convertTable;             // 类型转换表
} XL_TypeDesc;
```

### 4.3 类型注册 API

```c
// 类型表
typedef struct XL_TypeTable {
	XL_TypeDesc* types[1024];       // 类型数组
	int count;                      // 类型数量
	xvalue nameIndex;               // 名称->ID 索引 (xvalue Table)
} XL_TypeTable;

// 创建类型表
XL_TypeTable* xlTypeTableCreate(void);

// 销毁类型表
void xlTypeTableDestroy(XL_TypeTable* table);

// 注册类型
bool xlTypeRegister(XL_TypeTable* table, XL_TypeDesc* desc);

// 查找类型（按 ID）
XL_TypeDesc* xlTypeLookup(XL_TypeTable* table, int typeId);

// 查找类型（按名称）
XL_TypeDesc* xlTypeLookupByName(XL_TypeTable* table, const char* name);

// 是否为类型名
bool xlTypeIsTypeName(XL_TypeTable* table, const char* name);

// 注册所有标准类型
bool xlTypeRegisterStandard(XL_TypeTable* table);
```

### 4.4 类型注册实现模式

每个类型在独立的头文件中实现，遵循以下模式：

```c
// xl_type_int.h - int 类型注册

static void xlTypeInt_CodegenDecl(XL_CodegenContext* ctx, const char* varName, xvalue initExpr)
{
	if ( initExpr ) {
		xlCodegenPrintf(ctx, "int64_t %s = ", varName);
		xlCodegenExpr(ctx, initExpr);
		xlCodegenPrintf(ctx, ";\n");
	} else {
		xlCodegenPrintf(ctx, "int64_t %s = 0;\n", varName);
	}
}

static void xlTypeInt_CodegenConvert(XL_CodegenContext* ctx, xvalue expr, int targetType)
{
	switch ( targetType ) {
		case XL_TYPE_STRING:
			xlCodegenPrintf(ctx, "xrtIntToStr(");
			xlCodegenExpr(ctx, expr);
			xlCodegenPrintf(ctx, ")");
			break;
		case XL_TYPE_FLOAT:
			xlCodegenPrintf(ctx, "(double)(");
			xlCodegenExpr(ctx, expr);
			xlCodegenPrintf(ctx, ")");
			break;
		// ... 其他转换
	}
}

static void xlTypeInt_CodegenOp(XL_CodegenContext* ctx, const char* op, xvalue left, xvalue right)
{
	// 大多数运算符直接映射到 C
	xlCodegenPrintf(ctx, "(");
	xlCodegenExpr(ctx, left);
	xlCodegenPrintf(ctx, " %s ", op);
	xlCodegenExpr(ctx, right);
	xlCodegenPrintf(ctx, ")");
}

static XL_TypeDesc xlTypeDescInt = {
	.typeId = XL_TYPE_I64,
	.name = "int",
	.cType = "int64_t",
	.category = XL_TYPE_CAT_ATOMIC,
	.size = 8,
	.procCodegenDecl = xlTypeInt_CodegenDecl,
	.procCodegenConvert = xlTypeInt_CodegenConvert,
	.procCodegenOp = xlTypeInt_CodegenOp,
	.procCodegenIndex = NULL,       // int 不支持索引
	.procCodegenCall = NULL,        // int 不支持调用
};

void xlRegisterTypeInt(XL_TypeTable* table)
{
	xlTypeRegister(table, &xlTypeDescInt);
	// 同时注册别名
	xlTypeRegisterAlias(table, "i64", XL_TYPE_I64);
}
```

## 5. 代码生成器 (xl_codegen.h)

### 5.1 编译器上下文

```c
// 编译器上下文
typedef struct XL_Compiler {
	XL_TypeTable* typeTable;        // 类型表
	xvalue ast;                     // AST
	xvalue symbols;                 // 符号表 (xvalue Table)
	xvalue errors;                  // 错误列表
	
	// 代码生成
	char* codeBuffer;               // C 代码缓冲区
	size_t codeSize;                // 缓冲区大小
	size_t codeUsed;                // 已使用大小
	int indentLevel;                // 缩进级别
	
	// 选项
	bool debugMode;                 // 调试模式
	bool optimizeLevel;             // 优化级别
	const char* outputFile;         // 输出文件
} XL_Compiler;

// 创建编译器
XL_Compiler* xlCompilerCreate(void);

// 销毁编译器
void xlCompilerDestroy(XL_Compiler* compiler);

// 编译源码
bool xlCompilerCompile(XL_Compiler* compiler, const char* source, size_t length, const char* filename);

// 从文件编译
bool xlCompilerCompileFile(XL_Compiler* compiler, const char* filepath);

// 获取生成的 C 代码
const char* xlCompilerGetCode(XL_Compiler* compiler);

// 获取错误
xvalue xlCompilerGetErrors(XL_Compiler* compiler);
```

### 5.2 代码生成工具函数

```c
// 输出格式化代码
void xlCodegenPrintf(XL_CodegenContext* ctx, const char* fmt, ...);

// 输出缩进
void xlCodegenIndent(XL_CodegenContext* ctx);

// 增加/减少缩进
void xlCodegenIndentInc(XL_CodegenContext* ctx);
void xlCodegenIndentDec(XL_CodegenContext* ctx);

// 生成表达式
void xlCodegenExpr(XL_CodegenContext* ctx, xvalue expr);

// 生成语句
void xlCodegenStmt(XL_CodegenContext* ctx, xvalue stmt);

// 生成语句列表
void xlCodegenStmtList(XL_CodegenContext* ctx, xvalue stmts);

// 生成函数
void xlCodegenFunc(XL_CodegenContext* ctx, xvalue func);

// 生成程序
void xlCodegenProgram(XL_CodegenContext* ctx, xvalue program);
```

### 5.3 代码生成模板

```c
// C 代码模板 - 头部
static const char* XL_CODEGEN_HEADER =
	"/* Generated by xlang compiler */\n"
	"#define XRT_IMPLEMENTATION\n"
	"#include \"xrt.h\"\n"
	"\n"
	"/* xlang runtime */\n"
	"#include \"xl_runtime.h\"\n"
	"\n";

// C 代码模板 - 主函数入口
static const char* XL_CODEGEN_MAIN_START =
	"int main(int argc, char** argv)\n"
	"{\n"
	"\txlRuntimeInit();\n"
	"\txvalue __xl_args = xlArgsCreate(argc, argv);\n"
	"\n";

static const char* XL_CODEGEN_MAIN_END =
	"\n"
	"\txlRuntimeCleanup();\n"
	"\treturn 0;\n"
	"}\n";
```

## 6. 运行时库 (xl_runtime.h)

### 6.1 运行时初始化

```c
// 运行时初始化
void xlRuntimeInit(void);

// 运行时清理
void xlRuntimeCleanup(void);

// 创建命令行参数
xvalue xlArgsCreate(int argc, char** argv);
```

### 6.2 内置函数

```c
// 输出函数
void xlPrint(xvalue val);
void xlPrintln(xvalue val);

// 类型函数
int xlType(xvalue val);
const char* xlTypeName(xvalue val);
int xlLen(xvalue val);

// 类型转换
xvalue xlToInt(xvalue val);
xvalue xlToFloat(xvalue val);
xvalue xlToString(xvalue val);
xvalue xlToBool(xvalue val);

// 容器构造
xvalue xlArray(void);
xvalue xlList(void);
xvalue xlSet(void);
xvalue xlDict(void);

// 范围
xvalue xlRange(int64_t start, int64_t end, int64_t step);
```

## 7. TCC 集成 (xl_tcc.h)

### 7.1 TCC 编译器封装

```c
// TCC 编译器封装
typedef struct XL_TccCompiler {
	TCCState* state;                // TCC 状态
	char* errorBuffer;              // 错误信息
	size_t errorSize;
} XL_TccCompiler;

// 创建 TCC 编译器
XL_TccCompiler* xlTccCreate(void);

// 销毁 TCC 编译器
void xlTccDestroy(XL_TccCompiler* tcc);

// 编译 C 代码到内存
bool xlTccCompile(XL_TccCompiler* tcc, const char* code);

// 运行编译后的代码
int xlTccRun(XL_TccCompiler* tcc, int argc, char** argv);

// 编译到可执行文件
bool xlTccCompileToExe(XL_TccCompiler* tcc, const char* code, const char* outputPath);

// 获取错误信息
const char* xlTccGetError(XL_TccCompiler* tcc);
```

### 7.2 脚本模式实现

```c
// 脚本模式：编译并立即运行
int xlRunScript(const char* filepath, int argc, char** argv)
{
	// 1. 读取源文件
	char* source = xrtReadFile(filepath);
	
	// 2. 创建编译器
	XL_Compiler* compiler = xlCompilerCreate();
	
	// 3. 编译为 C 代码
	if ( !xlCompilerCompile(compiler, source, strlen(source), filepath) ) {
		// 输出错误
		xlPrintErrors(xlCompilerGetErrors(compiler));
		xlCompilerDestroy(compiler);
		free(source);
		return 1;
	}
	
	// 4. 获取 C 代码
	const char* cCode = xlCompilerGetCode(compiler);
	
	// 5. 使用 TCC 编译运行
	XL_TccCompiler* tcc = xlTccCreate();
	if ( !xlTccCompile(tcc, cCode) ) {
		printf("TCC 编译错误: %s\n", xlTccGetError(tcc));
		xlTccDestroy(tcc);
		xlCompilerDestroy(compiler);
		free(source);
		return 1;
	}
	
	int result = xlTccRun(tcc, argc, argv);
	
	// 6. 清理
	xlTccDestroy(tcc);
	xlCompilerDestroy(compiler);
	free(source);
	
	return result;
}
```

## 8. 命令行工具

### 8.1 xlc - xlang 编译器

```
用法: xlc [选项] <文件>

选项:
  -o <文件>     指定输出文件名
  -c            仅编译为 C 代码，不链接
  -S            输出生成的 C 代码到标准输出
  -O<级别>      优化级别 (0-3)
  -g            生成调试信息
  -v            显示详细信息
  -h            显示帮助

示例:
  xlc test.xl               编译 test.xl 为 test.exe
  xlc -o app.exe test.xl    编译为 app.exe
  xlc -c test.xl            生成 test.c
  xlc -S test.xl            打印生成的 C 代码
```

### 8.2 xl - xlang 解释器

```
用法: xl [选项] <文件> [参数...]

选项:
  -v            显示版本
  -h            显示帮助
  --tokens      仅显示 Token 列表
  --ast         仅显示 AST
  --code        仅显示生成的 C 代码

示例:
  xl test.xl                运行 test.xl
  xl test.xl arg1 arg2      运行 test.xl 并传递参数
  xl --ast test.xl          显示 AST
```

## 9. 构建脚本

### 9.1 build.bat (Windows TCC 快速编译)

```batch
@echo off
cd /d %~dp0

echo 编译 xlang 解释器...
tcc -o release\xl.exe main.c -Ilib -Llib\tcc -ltcc

echo 完成!
```

### 9.2 build_gcc.bat (Windows GCC 优化编译)

```batch
@echo off
cd /d %~dp0

echo 编译 xlang 解释器 (GCC 优化)...
gcc -o release\xl.exe main.c -Ilib -Llib\tcc -ltcc -O2

echo 编译 xlang 编译器...
gcc -o release\xlc.exe main.c -Ilib -Llib\tcc -ltcc -O2 -DXL_COMPILER_MODE

echo 完成!
```

### 9.3 build.sh (Linux)

```bash
#!/bin/bash
cd "$(dirname "$0")"

echo "编译 xlang 解释器..."
gcc -o release/xl main.c -Ilib -Llib/tcc -ltcc -O2

echo "编译 xlang 编译器..."
gcc -o release/xlc main.c -Ilib -Llib/tcc -ltcc -O2 -DXL_COMPILER_MODE

echo "完成!"
```

## 10. 测试用例

### 10.1 Hello World

```xlang
// hello.xl
println("Hello, xlang!")
```

### 10.2 变量和运算

```xlang
// vars.xl
int a = 10
int b = 20
int c = a + b
println("a + b = " + string(c))

var x = 3.14
var y = "hello"
println(type(x))  // float
println(type(y))  // string
```

### 10.3 控制流

```xlang
// control.xl
int n = 10

if n > 0 then
	println("正数")
elseif n < 0 then
	println("负数")
else
	println("零")
end

for i = 1 to 10
	println(i)
end

var arr = [1, 2, 3, 4, 5]
for v in arr
	println(v)
end
```

### 10.4 函数

```xlang
// func.xl
func factorial(int n) as int
	if n <= 1 then
		return 1
	end
	return n * factorial(n - 1)
end

println(factorial(5))  // 120

func greet(string name, string msg = "Hello")
	println(msg + ", " + name + "!")
end

greet("World")           // Hello, World!
greet("xlang", "Welcome") // Welcome, xlang!
```

### 10.5 容器类型

```xlang
// containers.xl
var arr = [1, 2, 3]
arr.append(4)
println(arr)  // [1, 2, 3, 4]

var dict = {"name": "xlang", "version": 4}
println(dict["name"])  // xlang

var set_a = set{1, 2, 3}
var set_b = set{2, 3, 4}
println(set_a & set_b)  // {2, 3}
```

### 10.6 带参数的 main

```xlang
// args.xl
func main(...) as int
	println("参数数量: " + string(len(arg)))
	for i, v in arg
		println("arg[" + string(i) + "] = " + v)
	end
	return 0
end
```

## 11. 分步实现计划

阶段三将分为多个子步骤，每步需用户确认：

### 11.1 步骤 1：类型系统核心
- [ ] 实现 XL_TypeTable
- [ ] 实现 XL_TypeDesc
- [ ] 实现类型查找 API
- [ ] 用户确认：类型系统核心

### 11.2 步骤 2：内置类型注册
- [ ] 实现 null 类型
- [ ] 实现 function 类型
- [ ] 用户确认：内置类型

### 11.3 步骤 3：原子类型注册
- [ ] 实现 bool 类型
- [ ] 实现 int 系列类型
- [ ] 实现 float 系列类型
- [ ] 实现 time 类型
- [ ] 实现 point 类型
- [ ] 用户确认：原子类型

### 11.4 步骤 4：复合类型注册
- [ ] 实现 string 类型
- [ ] 用户确认：复合类型

### 11.5 步骤 5：容器类型注册
- [ ] 实现 array 类型
- [ ] 实现 list 类型
- [ ] 实现 set 类型
- [ ] 实现 dict 类型
- [ ] 用户确认：容器类型

### 11.6 步骤 6：特殊类型注册
- [ ] 实现 var 类型
- [ ] 实现 type 类型
- [ ] 实现 range 类型
- [ ] 用户确认：特殊类型

### 11.7 步骤 7：代码生成器核心
- [ ] 实现 XL_Compiler
- [ ] 实现代码缓冲区
- [ ] 实现缩进管理
- [ ] 用户确认：代码生成器核心

### 11.8 步骤 8：表达式代码生成
- [ ] 字面量代码生成
- [ ] 标识符代码生成
- [ ] 二元表达式代码生成
- [ ] 一元表达式代码生成
- [ ] 函数调用代码生成
- [ ] 索引/成员访问代码生成
- [ ] 用户确认：表达式代码生成

### 11.9 步骤 9：语句代码生成
- [ ] 变量声明代码生成
- [ ] 赋值语句代码生成
- [ ] if 语句代码生成
- [ ] for 循环代码生成
- [ ] while 循环代码生成
- [ ] break/continue/return 代码生成
- [ ] 用户确认：语句代码生成

### 11.10 步骤 10：函数代码生成
- [ ] 函数声明代码生成
- [ ] 参数处理代码生成
- [ ] 返回值代码生成
- [ ] 可变参数代码生成
- [ ] 用户确认：函数代码生成

### 11.11 步骤 11：运行时库
- [ ] 实现 xl_runtime.h
- [ ] 实现内置函数
- [ ] 用户确认：运行时库

### 11.12 步骤 12：TCC 集成
- [ ] 实现 TCC 封装
- [ ] 实现脚本模式
- [ ] 实现编译模式
- [ ] 用户确认：TCC 集成

### 11.13 步骤 13：命令行工具
- [ ] 实现 xlc 编译器
- [ ] 实现 xl 解释器
- [ ] 用户确认：命令行工具

### 11.14 步骤 14：综合测试
- [ ] 运行所有测试用例
- [ ] 修复发现的问题
- [ ] 用户最终验收

## 12. 验收检查清单

- [ ] 类型注册系统工作正常
- [ ] 所有 v4 一阶段类型已注册
- [ ] 代码生成器能生成正确的 C 代码
- [ ] 生成的代码能通过 TCC 编译
- [ ] 生成的代码能通过 GCC 编译
- [ ] 脚本模式能正常运行
- [ ] 编译模式能生成可执行文件
- [ ] 所有测试用例通过
- [ ] 错误信息清晰准确
- [ ] 内存无泄漏
- [ ] 用户完全理解代码结构
