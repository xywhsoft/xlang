# DEMO6 xl.dll / xlc.dll 嵌入式 API

本文档记录 DEMO6 嵌入式 DLL API 的当前口径。DLL 面向“把 xlang 作为脚本语言嵌入宿主程序”的场景，不承载 CLI 调试命令。

## 产物分层

`xl.dll` 是轻量运行版：

- 创建和销毁语言上下文。
- 设置上下文工作目录和临时目录。
- 运行字符串脚本。
- 运行脚本文件。
- 接收宿主提供的 `xvalue arg` 并调用固定 `xlang_main(xvalue arg)` 入口。
- 添加 include path、library path、链接库或依赖 DLL。
- 将宿主 C 函数注册到上下文，供 xlang `extern "symbol"` 声明调用。
- 自动把 DLL 内部链接的 XRT 符号注入 libtcc 内存执行环境。

`xlc.dll` 是编译版 DLL：

- 支持 `xl.dll` 的全部运行能力。
- 额外导出 `xlCompileScriptStringToExe`。
- 额外导出 `xlCompileScriptFileToExe`。
- 后续负责携带编译 exe/dll 所需的内置 TCC/XRT 资源包。

`xl.dll` 不导出编译 exe API。宿主如果只面向运行版，可以在包含 `include/xl.h` 前定义 `XL_RUNTIME_ONLY_API`，此时头文件不会声明编译 API。

两个 DLL 都不提供：

- 词法 dump。
- 语法树 dump。
- AST/IR/ABI 调试输出命令。
- CLI 参数解析。
- 交互式调试器。

## 公共头文件

公共头文件为：

```c
#include "include/xl.h"
```

运行版核心 API：

```c
XL_ContextOptions xlDefaultContextOptions(void);
int xlRuntimeInit(void);
void xlRuntimeUnit(void);

XL_Context* xlCreateContext(const XL_ContextOptions* options);
void xlDestroyContext(XL_Context* ctx);

int xlSetWorkDir(XL_Context* ctx, const char* path);
int xlSetTempDir(XL_Context* ctx, const char* path);
int xlSetSaveTemps(XL_Context* ctx, int enabled);

int xlAddIncludePath(XL_Context* ctx, const char* path);
int xlAddLibraryPath(XL_Context* ctx, const char* path);
int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath);
int xlAddResourceFile(XL_Context* ctx, const char* path);
int xlLoadDependencyLibrary(XL_Context* ctx, const char* path);
int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn);

int xlRunScriptString(XL_Context* ctx, const char* source);
int xlRunScriptFile(XL_Context* ctx, const char* path);
int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue arg);
int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue arg);

XL_Module* xlCompileScriptStringToModule(XL_Context* ctx, const char* source);
XL_Module* xlCompileScriptFileToModule(XL_Context* ctx, const char* path);
XL_Module* xlCompileCSourceStringToModule(XL_Context* ctx, const char* source);
XL_Module* xlCompileCSourceFileToModule(XL_Context* ctx, const char* path);
XL_Module* xlCompileCSourceFilesToModule(XL_Context* ctx, const char** paths, int pathCount);
int xlRunCSourceString(XL_Context* ctx, const char* source, int argc, char** argv);
int xlRunCSourceFile(XL_Context* ctx, const char* path, int argc, char** argv);
int xlRunCSourceFiles(XL_Context* ctx, const char** paths, int pathCount, int argc, char** argv);
int xlActivateModule(XL_Context* ctx, XL_Module* module, xvalue arg);
int xlDeactivateModule(XL_Context* ctx, XL_Module* module);
int xlRunModuleMain(XL_Context* ctx, XL_Module* module, xvalue arg);
void* xlModuleGetSymbol(XL_Module* module, const char* symbolName);
void xlDestroyModule(XL_Module* module);
int xlTccRegisterXrtEnvironment(TCCState* state);
int xlTccRegisterContextEnvironment(XL_Context* ctx, TCCState* state);
int xlTccImpdef(const char* dllPath, const char* outputDef);

const char* xlGetLastError(XL_Context* ctx);
void xlClearLastError(XL_Context* ctx);
int xlGetLastExitCode(XL_Context* ctx);
```

`xlRunModuleMain` 用于一次性脚本执行，按顺序完成模块初始化、用户入口和模块卸载。
需要长期保留模块全局状态的宿主使用 `xlActivateModule`，然后通过 `xlModuleGetSymbol` 调用导出函数，最后使用 `xlDeactivateModule` 成对卸载。重复激活和重复卸载均为幂等操作；活动模块不能同时作为一次性脚本运行，`xlDestroyModule` 会对仍处于活动状态的模块执行兜底卸载。

编译版额外 API：

```c
int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe);
int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe);
int xlCompileCSourceFileToExe(XL_Context* ctx, const char* path, const char* outputExe);
int xlCompileCSourceFilesToExe(XL_Context* ctx, const char** paths, int pathCount, const char* outputExe);
int xlCompileCSourceFileToSharedLibrary(XL_Context* ctx, const char* path, const char* outputDll);
int xlCompileCSourceFilesToSharedLibrary(XL_Context* ctx, const char** paths, int pathCount, const char* outputDll);
```

Compiler library header:
```c
#include "include/xlc.h"
```

`xlc.dll` compiler library API:
```c
typedef enum XLC_Backend {
    XLC_BACKEND_XLIR = 0
} XLC_Backend;

XLC_ContextOptions xlcDefaultContextOptions(void);
XLC_Context* xlcCreateContext(const XLC_ContextOptions* options);
void xlcDestroyContext(XLC_Context* ctx);

int xlcSetWorkDir(XLC_Context* ctx, const char* path);
int xlcSetTempDir(XLC_Context* ctx, const char* path);
int xlcSetSaveTemps(XLC_Context* ctx, int enabled);
int xlcSetBackend(XLC_Context* ctx, XLC_Backend backend);
int xlcAddIncludePath(XLC_Context* ctx, const char* path);
int xlcAddLibraryPath(XLC_Context* ctx, const char* path);
int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath);
int xlcAddCSourceFile(XLC_Context* ctx, const char* path);
int xlcAddResourceFile(XLC_Context* ctx, const char* path);
int xlcLoadDependencyLibrary(XLC_Context* ctx, const char* path);
int xlcRegisterCFunction(XLC_Context* ctx, const char* symbolName, void* fn);

int xlcAddSourceFile(XLC_Context* ctx, const char* path);
int xlcAddSourceString(XLC_Context* ctx, const char* virtualPath, const char* source);
int xlcCompile(XLC_Context* ctx);
int xlcCheck(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitTokens(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitAst(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitLoweredIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitC(XLC_Context* ctx, const char* outputPath);
int xlcEmitXlirC(XLC_Context* ctx, const char* outputPath);
int xlcEmitDebugC(XLC_Context* ctx, const char* outputPath, const char* debugSourcePath);
int xlcEmitHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
int xlcEmitXlirHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
int xlcEmitAbi(XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitExe(XLC_Context* ctx, const char* outputExe);
int xlcEmitDebugExe(XLC_Context* ctx, const char* outputExe, const char* debugSourcePath);
int xlcEmitSharedLibrary(XLC_Context* ctx, const char* outputDll);
XL_Module* xlcRelocateModule(XLC_Context* ctx);
int xlcRun(XLC_Context* ctx, int argc, char** argv);

XL_Context* xlcRuntimeContext(XLC_Context* ctx);
const char* xlcGetLastError(XLC_Context* ctx);
void xlcClearLastError(XLC_Context* ctx);
int xlcGetLastExitCode(XLC_Context* ctx);
```

`xlc.dll` owns compiler-library operations that are useful without CLI argument parsing: checking, token/AST/IR/ABI emission, C/header/debug-C emission, executable/debug-executable/shared-library output, in-process relocation, and direct run. `xl.exe` is responsible for command-line parsing and maps those commands onto these APIs.

XLIR 是唯一编译后端。`xlcSetBackend` 仅为显式选择和 ABI 兼容保留，传入 `XLC_BACKEND_XLIR` 返回成功；其他枚举值返回 `0` 并写入 context 诊断。公开调用仍受 DLL API 的全局串行锁保护；宿主不应并发修改同一 context 的配置和执行编译。

`xlcEmitIr` 输出语义检查后的 typed XLIR，`xlcEmitLoweredIr` 则先完成显式 lowering，再以文本或 JSON 输出可供后端消费的 lowered XLIR。默认 `xlcEmitC` 服从 context 的后端选择；`xlcEmitXlirC` 和 `xlcEmitXlirHeader` 始终只走统一 XLIR 后端。XLIR 路径都会完成编译及 lowering，并由 verifier 拒绝未确定的 C ABI、所有权或控制流语义。`xlcEmitXlirHeader` 要求非空 include guard。上述函数在上下文、路径、lowering、验证或文件读写失败时返回 `0`，具体诊断由 `xlcGetLastError` 提供。

命名说明：

- `xlRuntimeInit` / `xlRuntimeUnit` 是进程级公共 API，用于管理 DLL 内部 XRT/libtcc 运行时计数。
- `xlCreateContext` / `xlDestroyContext` 会为每个 context 持有独立运行时引用；公开 `xlRuntimeUnit` 只释放公开 `xlRuntimeInit` 取得的引用，不能提前释放仍被 context 持有的运行时。
- 重定位得到的 `XL_Module` 独立持有运行时引用和 context 已加载依赖库的加载引用。因此可以先销毁创建它的 context，再继续调用模块符号，直到 `xlDestroyModule`。
- 最后一个运行时引用只能在 TCC state、依赖库记录和模块/context 外壳等所有 XRT 分配对象释放后归还；不得在 `xrtUnit()` 完成内存池收尾后再次调用 `xrtFree()`。
- `xlAddResourceFile` / `xlcAddResourceFile` 登记 Windows 资源配置文件。编译时资源配置被转换为 COFF `.rsrc` 对象并挂载到动态 VFS，再交给 TCC 链接，不生成磁盘 `.obj` 中间文件。
- `xlLoadDependencyLibrary` 加载的依赖库句柄由当前 context 持有，`xlDestroyContext` 负责释放；该 API 会在调用时把路径按宿主当前目录固化为快照，然后用同一快照路径加载动态库并参与后续 TCC 链接。
- 生成出来的 xlang/C 程序仍使用 `xlInit` / `xlUnit` 作为模块生命周期入口。
- DEMO6 不使用任何 Uninit/UnInit 式卸载命名，避免和 `xrtInit` / `xrtUnit` 口径冲突。

## 运行模型

字符串脚本和文件脚本都走统一流水线：

1. parse
2. import expansion
3. semantic check
4. C code generation
5. libtcc compile
6. relocate
7. get `xlang_main`
8. call `xlang_main(xvalue arg)`

进程内运行时，DLL 不额外链接外部 `xrt.o`。它会将自身内部链接的 XRT 函数和宿主注册的 C 函数注入 TCC state。

独立 exe 输出只由 `xlc.dll` 负责。目标 exe 的 XRT 实现由编译版 VFS 中的单头文件 `lib/xrt/xrt.h` 提供，`xlc.dll` 不再要求外部 `xrt.o`。

`xl.dll` 与 `xlc.dll` 均静态内嵌 libtcc，不再搜索或复制外部 `libtcc.dll`。`XL_ContextOptions.tccDllPath` 作为公开结构体字段保留，但当前静态 libtcc 构建不会使用它加载动态库。

TCC lib root 默认优先使用 context 显式 `tccLibPath`，否则读取 `XL6_TCC_ROOT`，未设置时回退到 `XL6_TCC_ROOT_PATH`。TCC include、Win include、WinAPI include、library path 和 libtcc runtime 分别优先读取 `XL6_TCC_INCLUDE`、`XL6_TCC_WIN_INCLUDE`、`XL6_TCC_WINAPI_INCLUDE`、`XL6_TCC_LIB`、`XL6_TCC_LIBTCC1`，未设置时回退到 `xl6_toolchain.h` 中的源码级默认值。

`xlCreateContext()` 会记录创建 context 时的当前目录，并把 `workDir`、`tempDir`、显式或默认 TCC 路径按该目录做路径快照。`xlSetWorkDir()` 和 `xlSetTempDir()` 也会按调用当下的当前目录做路径快照。

手动追加的 include/library/link/loadlib 按 API 调用时的路径环境固定。`xlAddIncludePath` 和 `xlAddLibraryPath` 会把相对路径按宿主当前目录固化为快照；`xlAddLinkLibrary` 会保留裸库名，让它继续走 TCC library 搜索规则，但会把像路径的值固化为快照；`xlLoadDependencyLibrary` 会用快照路径加载动态库并记录到 context。

## 工作目录

上下文工作目录用于：

- 脚本文件相对路径解析。
- 字符串脚本的相对 `import` 解析。
- `packages` / `lib` 包搜索基准。

字符串脚本使用逻辑文件名 `__xl_string.xl`，因此相对 import 会以当前上下文工作目录为基准。

## 临时目录

上下文临时目录由 `XL_ContextOptions.tempDir` 或 `xlSetTempDir` 设置。相对路径会在创建 context 或调用 `xlSetTempDir` 时按当时的宿主当前目录固化为路径快照。

当前 DLL 执行管线以动态 VFS + libtcc 内存编译为主，不输出 CLI 调试 dump，普通运行或编译 exe 不会为了主生成 C 预创建临时目录。`xlc.dll` 编译版 API 在启用 `saveTemps` 时，会把动态 VFS 中的 `module.c`、`module.h` 和 `csrc_N.c` 等可检查副本导出到 `tempDir`；未启用时不落盘。

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
extern "host_add" func hostAdd(int a, int b) as int

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
test_xlc_shell.bat
```

验证程序包括：

```text
release\examples\xl_dll_null_context.c
release\examples\xl_dll_threaded_host.c
release\examples\xl_dll_embed_string.c
release\examples\xl_dll_xrt_exports.c
release\examples\xlc_api_embed.c
release\examples\xl_run_demo_win32.c
```

其中：

- `xl_dll_null_context.c` 链接 `xl.def`，验证运行版 API 的 NULL context、cwd、路径快照和生命周期边界。
- `xl_dll_threaded_host.c` 链接 `xl.def`，验证运行版 API 的多线程宿主边界。
- `xl_dll_embed_string.c` 链接 `xlc.def`，验证字符串脚本运行、文件脚本运行、编译字符串脚本到 exe、编译文件脚本到 exe。
- `test/cases/xl_dll_module_dependency_lifetime.c` 验证重定位模块在 context 销毁后仍保活动态依赖，并以测试侧看门线程阻止生命周期死锁无限挂住回归。

## 当前约束

- `xl.dll` 和 `xlc.dll` 不生成、不复制、不依赖外部 `libtcc.dll`。`xl.dll` 携带运行版 VFS 小清单，`xlc.dll` 携带完整编译版 VFS 清单；外部 TCC include/lib 等路径存在时仍可覆盖内置兜底资源。
- 运行版 VFS 清单只允许携带 TCC include 目录和 `lib/libtcc1.a`。`xrt` 由 `tcc_add_symbol` 导入，运行版清单不能携带 `lib/xrt/xrt.h`、`lib/xrt/xrt.o` 或 exe 链接所需的 `.def` 资源。
- 编译版 VFS 清单必须携带 exe 链接所需的 `.def` 资源和 `lib/xrt/xrt.h`，但不能携带 `xrt.o`。编译 exe 时通过 `#define XRT_IMPLEMENTATION` 加载单头文件版 XRT。
- `tools/check_dll_api_boundary.ps1` 会静态检查 runtime/xlc 两份 manifest 的职责边界，并确认 `build_dll.bat` 没有把两套内置资源用反。
- 进程内运行适合脚本执行和宿主 C 函数注册。
- 编译为独立 exe 时，不能直接固化宿主进程函数指针；需要通过普通 C/库链接方式提供外部符号。
- DLL API 第一版不暴露调试 dump 能力，相关能力继续由 `xl.exe` CLI 提供。
