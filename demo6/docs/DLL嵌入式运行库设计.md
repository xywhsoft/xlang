# DEMO6 xl.dll 嵌入式运行库

本文档记录 `xl.dll` 的第一版嵌入式 API 口径。DLL 面向“把 xlang 作为脚本语言嵌入宿主程序”的场景，不承载 CLI 调试命令。

## 设计边界

`xl.dll` 提供：

- 创建和销毁语言上下文。
- 设置上下文工作目录和临时目录。
- 运行字符串脚本。
- 运行脚本文件。
- 将字符串脚本编译为 exe。
- 将脚本文件编译为 exe。
- 添加 include path、library path、链接库或依赖 DLL。
- 将宿主 C 函数注册到上下文，供 xlang `extern "symbol"` 声明调用。
- 自动把 `xl.dll` 内部链接的 XRT 符号注入 libtcc 内存执行环境。

`xl.dll` 不提供：

- 词法 dump。
- 语法树 dump。
- AST/IR/ABI/XAPI 调试输出命令。
- CLI 参数解析。
- 交互式调试器。

## 公共头文件

公共头文件为：

```c
#include "include/xl.h"
```

核心 API：

```c
XL_ContextOptions xlDefaultContextOptions(void);
int xlRuntimeInit(void);
void xlRuntimeUnit(void);

XL_Context* xlCreateContext(const XL_ContextOptions* options);
void xlDestroyContext(XL_Context* ctx);

int xlSetWorkDir(XL_Context* ctx, const char* path);
int xlSetTempDir(XL_Context* ctx, const char* path);

int xlAddIncludePath(XL_Context* ctx, const char* path);
int xlAddLibraryPath(XL_Context* ctx, const char* path);
int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath);
int xlLoadDependencyLibrary(XL_Context* ctx, const char* path);
int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn);

int xlRunScriptString(XL_Context* ctx, const char* source);
int xlRunScriptFile(XL_Context* ctx, const char* path);
int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe);
int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe);

const char* xlGetLastError(XL_Context* ctx);
void xlClearLastError(XL_Context* ctx);
```

命名说明：

- `xlRuntimeInit` / `xlRuntimeUnit` 是 `xl.dll` 的进程级公共 API，用于管理 DLL 内部 XRT/libtcc 运行时计数。
- 生成出来的 xlang/C 程序仍使用 `xlInit` / `xlUnit` 作为模块生命周期入口。
- DEMO6 不使用任何 Uninit/UnInit 式卸载命名，避免和 `xrtInit` / `xrtUnit` 口径冲突。

## 运行模型

字符串脚本和文件脚本都走统一流水线：

1. parse
2. import expansion
3. sem check
4. C code generation
5. libtcc compile
6. run in memory or output exe

进程内运行时，`xl.dll` 不额外链接外部 `xrt.o`。它会将自身内部链接的 XRT 函数和宿主注册的 C 函数注入 TCC state。

独立 exe 输出时，目标 exe 仍需要静态链接 XRT object；DLL API 会按当前 DEMO6 约定自动使用 `..\..\xrt\release\x64\xrt.o`。

## 工作目录

上下文工作目录用于：

- 脚本文件相对路径解析。
- 字符串脚本的相对 `import` 解析。
- `packages` / `lib` 包搜索基准。

字符串脚本使用逻辑文件名 `__xl_string.xl`，因此相对 import 会以当前上下文工作目录为基准，而不是临时目录。

## C 函数注册

宿主通过 `xlRegisterCFunction` 注册 C 函数：

```c
static long long host_add(long long a, long long b)
{
    return a + b;
}

xlRegisterCFunction(ctx, "host_add", (void*)host_add);
```

xlang 侧显式声明后使用：

```xlang
extern "host_add" func hostAdd(int a, int b) -> int

int value = hostAdd(20, 22)
assert value == 42, "host function call failed"
```

这保持了 xlang 环境干净：C 函数不会自动污染 xlang 命名空间，必须由脚本显式声明。

## 构建与验证

构建 DLL：

```bat
build_dll.bat
```

专项验证：

```bat
test_dll.bat
```

验证程序位于：

```text
examples\xl_dll_embed_string.c
```

它覆盖：

- 创建 `XL_Context`。
- 注册宿主 C 函数。
- 直接运行字符串脚本。
- 将字符串脚本编译为 exe。
- 运行生成出的 exe。

## 当前约束

- `xl.dll` 当前使用 libtcc 动态库，构建时会复制 `libtcc.dll` 到 `release` 目录。
- 进程内运行适合脚本执行和宿主 C 函数注册。
- 编译为独立 exe 时，不能直接固化宿主进程函数指针；需要通过普通 C/库链接方式提供外部符号。
- DLL API 第一版不暴露调试 dump 能力，相关能力继续由 `xl.exe` CLI 提供。
