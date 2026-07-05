# TCC 内嵌与运行入口设计

本文记录 DEMO6 后续把 `libtcc` 内嵌到 `xl.exe` / `xl.dll` 的统一口径，以及当前已经落地的运行入口改造。

## 目标

- `xl.exe` 单文件具备脚本运行、`.xl` 编译为 exe/dll、`.c` 混编、关键 TCC 工具能力。
- `xl.dll` 拆分为运行版和编译版两个职责：
  - 运行版只支持宿主进程内创建语言上下文、运行字符串脚本或脚本文件、注册宿主 C 函数。
  - 编译版在运行版能力之外支持编译 exe/dll，并携带必要的内置 TCC/XRT 资源。
- 运行版携带运行所需的 VFS 小清单，不携带 exe/dll 输出所需的 import def、XRT object 等编译资源，保持体积小。
- 运行版、编译版和 `xl.exe` 都使用清单驱动的 VFS，不再扫描目录后把所有文件塞进程序。
- VFS 分为动态 VFS 和静态 VFS：动态 VFS 保存本次编译临时生成物，静态 VFS 保存内置 TCC/XRT 兜底资源。
- TCC 文件查找顺序固定为：动态 VFS -> 外部文件系统 -> 静态 VFS。

## 产物职责

`xl.exe`：
- 面向命令行用户的完整编译器和脚本运行入口。
- 内嵌 `libtcc`。
- 内嵌编译所需资源包。
- 提供关键 TCC 工具能力，例如 DLL 导出表生成 def。

`xl.dll`：
- 轻量运行版。
- 支持 `XL_Context`、运行字符串脚本、运行脚本文件、工作目录、依赖库导入、宿主 C 函数注册。
- 静态内嵌 libtcc，并只内嵌运行 generated C 所需的 TCC include 与 libtcc1 资源。
- XRT 与宿主注册函数通过 `tcc_add_symbol` 注入 TCC 内存执行环境。

`xlc.dll`：
- 编译版 DLL。
- 支持 `xl.dll` 的运行能力。
- 额外支持编译 exe/dll。
- 内嵌编译所需资源包。

## 统一入口 ABI

xlang 源码中的入口固定降低为：

```c
int xlang_main(xvalue arg);
```

它对应 xlang 侧：

```xlang
func main(...) as int
    return 0
end
```

如果 xlang `main` 没有显式返回值，生成器仍然生成 `int xlang_main(xvalue arg)`，并在函数尾部补 `return 0;`。

编译为 exe 时，生成的 C `main` 只做以下工作：

1. 调用生成代码的 `xlInit`。
2. 将 `argc/argv` 转为 `xvalue array`。
3. 调用 `xlang_main(arg)`。
4. 释放 `arg`。
5. 调用生成代码的 `xlUnit`。
6. 返回 `xlang_main` 的返回值。

DLL / 内嵌运行路径不调用 C `main`，而是：

1. 将生成 C 挂载到动态 VFS，例如 `__xlvfs__/module.c`。
2. `tcc_add_file("__xlvfs__/module.c")` 编译生成 C。
3. `tcc_relocate()` 完成重定位。
4. `tcc_get_symbol("xlang_main")` 获取入口。
5. 直接调用 `xlang_main(arg)`。

## 运行版参数

运行版公开两组 API：

```c
int xlRunScriptString(XL_Context* ctx, const char* source);
int xlRunScriptFile(XL_Context* ctx, const char* path);

int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue arg);
int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue arg);
```

无 `Arg` 版本会创建空 `array` 作为 `arg`。

带 `Arg` 版本要求宿主传入的 `xvalue` 必须是 `array`。调用期间 DLL 会增加一次引用计数，调用完成后释放这次引用，宿主仍然拥有原本的引用。

## XRT 原则

`xrt` 是 xlang 和 C 共同使用的基础运行库，不是 xlang 私有实现。

要求：
- 运行版、编译版、宿主 C 代码应尽量共享同一套 XRT 语义。
- `xvalue` 是公开 C ABI 的一部分，不应复制出第二套私有 value。
- 后续需要正式补齐 `xl.dll` 对 XRT/xvalue 公共 ABI 的导出策略，避免宿主为了构造 `xvalue arg` 静态链接第二份 XRT。

## TCC IO 补丁边界

`libtcc` 的改动必须尽量小，便于未来同步上游或外部实验仓库的更新。

DEMO6 固定在 `demo6/lib/tcc` 中维护本版使用的 TCC 副本。后续修改优先落在这个目录，`D:\GIT\tcc_xlang` 只作为外部实验来源或同步来源，不再作为 demo6 构建时的直接依赖。

推荐方式是在 `tcc.h` 或集中头文件中增加宏开关：

```c
#ifdef CONFIG_TCC_BUILTIN_VFS
#include "tcc_builtin_vfs.h"
#define open  tcc_vfs_open
#define read  tcc_vfs_read
#define lseek tcc_vfs_lseek
#define close tcc_vfs_close
#define fopen tcc_vfs_fopen
#elif defined(_WIN32)
#include "tcc_utf8_io.h"
#define open  tcc_utf8_open
#define fopen tcc_utf8_fopen
#endif
```

具体路径处理、UTF-8 到 Windows 宽字符路径转换、动态 VFS、外部文件和静态 VFS fallback 都放在独立适配层，不散落修改 TCC 主体代码。

## VFS 查找顺序

TCC 通过 `open/read/lseek/close/fopen/fclose` 适配层访问文件。DEMO6 固定以下查找顺序：

```text
动态 VFS -> 外部文件系统 -> 静态 VFS
```

动态 VFS：
- 保存本次编译生成的中间文件，例如 `__xlvfs__/module.c`、`__xlvfs__/runtime.c`。
- 生命周期跟一次编译或运行调用绑定，进入 TCC 前挂载，调用结束后清空。
- 优先级高于外部文件系统，因为它表达的是当前编译上下文刚刚生成的权威内容。
- 只作为读路径参与 TCC 编译，不承接 exe/dll/header 等最终输出写入。

外部文件系统：
- 用户真实提供的 `.c`、`.h`、`.a`、`.def`、`.dll` 等文件仍可被 TCC 正常读取。
- 当外部文件存在时，它优先于静态 VFS，允许用户用更新后的头文件、库文件或启动文件覆盖内置资源。

静态 VFS：
- 保存 `xl.exe` / `xlc.dll` 内置的 TCC/XRT 资源。
- 只作为兜底资源使用，保证缺少外部工具链文件时仍可零配置编译和运行。

正常编译运行不应为了交换中间 C 文件创建磁盘临时文件。只有用户显式要求输出内容时才写真实文件，例如：

- `xl cgen --emit output.c`
- `xl cgen --emit-header output.h`
- `xl cgen --dump-dir dir`
- `xl build -o app.exe`
- 后续 `saveTemps` 导出动态 VFS 内容

## VFS 资源包

内置资源使用虚拟路径，例如：

```text
include/stdio.h
include/winapi/windows.h
lib/libtcc1.a
lib/xrt/xrt.h
```

资源组建议：

- `tcc_include.lzma`
- `tcc_winapi_include.lzma`
- `tcc_lib.lzma`
- `xrt_header.lzma`
- `xrt_object.lzma`（仅 `xl.exe` 完整清单需要；`xlc.dll` 使用 `xrt.h` 单头文件实现）

资源清单记录：
- 虚拟路径。
- 原始大小。
- 压缩大小。
- 压缩算法。

资源生成必须清单驱动，不允许“扫描目录全塞”。

当前实现：
- 清单文件：`tools/tcc_vfs_manifest_win64_runtime.txt` 与 `tools/tcc_vfs_manifest_win64_xlc.txt`。
- 生成器：`tools/gen_tcc_vfs_resources.ps1`。
- 生成产物：`build\tmp\tcc_builtin_resources.c`，构建时临时生成，不作为手写源维护。
- VFS 适配层位于 `lib/tcc/tcc_builtin_vfs.c` / `lib/tcc/tcc_builtin_vfs.h`，策略是动态 VFS 优先，其次外部真实文件，最后按虚拟路径查找内置资源。
- 当前资源逐个压缩后进入 `xl.exe` / `xlc.dll`。资源结构包含 `method`、`packed_size`、`size`。
- 当前压缩算法固定为 LZMA：构建期使用 `tools/tcc_vfs_lzma_pack.c` 压缩，运行期只链接 `lib/lzma/LzmaDec.c` 解压；压缩后不变小的资源自动退回原样存储。
- `lib/tcc/win32/dist/x86_64` 已补齐当前 demo6 需要的 WinAPI 头文件、`libtcc1.a` 和 Windows import def。VFS 资源生成不再引用 demo5 或外部 TCC 仓库。
- `build.bat` 默认先用 MinGW `gcc` 从 `lib/tcc` 自举 `build\tmp\tcc_bootstrap.exe`，再用该本地 TCC 构建 `xl.dll`、`xlc.dll` 和 `xl.exe`；`build.sh` 是 Windows Git Bash/MSYS 下的同路径包装入口，不维护第二套构建逻辑。
- `build_dll.bat` 默认从 `lib/xrt/xrt.h` 生成 `build\tmp\xrt.o`，不再要求 `D:\GIT\xrt\release\x64\xrt.o` 预先存在。
- `src/lexer/xl6_lexer_gen.c` / `src/lexer/xl6_lexer_gen.h` 与 `src/parser/xl6_parser.c` / `src/parser/xl6_parser.h` 作为发布构建输入随仓库保存；只有设置 `XL6_REGEN_GRAMMAR=1` 时才调用 Flex/Lemon 重新生成。

## 压缩策略

当前实现采用单一 LZMA 压缩方案，不保留 LZ77 或多套兼容回退。

原则：
- 构建期允许使用 LZMA encoder，但 encoder 不进入 `xl.exe` / `xlc.dll`。
- 运行期只链接 LZMA decoder，当前为 `lib/lzma/LzmaDec.c`。
- 解压只发生在资源命中时，不是热路径。
- 运行版 `xl.dll` 只携带运行版 VFS 小清单，并链接 LZMA decoder；编译版 `xlc.dll` 和 `xl.exe` 携带完整编译清单。
- 压缩后不变小的资源仍可用 `STORE` 原样存储；这不是第二套压缩算法，只是资源表的基础存储形态。

本地测算结果：
- `xlc.dll` 编译版 VFS 原始资源约 4.99 MB，LZMA 后约 0.81 MB。
- 运行版 VFS 原始资源约 1.70 MB，LZMA 后约 0.30 MB。
- `xl.exe` 当前约 4.12 MB。
- `xlc.dll` 当前约 3.97 MB。
- 轻量运行版 `xl.dll` 当前约 3.47 MB。

## 当前已落地

- `func main(...)` 生成 `int xlang_main(xvalue arg)`。
- 顶层运行语句生成 `int xlang_main(xvalue arg)`。
- 生成的 C `main` 调用 `xlang_main(__xl6_arg)`。
- 生成的 C `main` 在正常和错误路径释放 `__xl6_arg`。
- DLL/API 与 CLI `xl run` 运行路径均不调用 C `main`，统一使用 `TCC_OUTPUT_MEMORY`、`tcc_relocate()`、`tcc_get_symbol("xlang_main")` 并直接传入 `xvalue arg`。
- `xlRunScriptString/File` 和 `xlCompileScriptString/FileToExe` 的主生成 C 已改为挂载动态 VFS 后通过 `tcc_add_file` 编译，不再用磁盘临时文件交换主 C 文件。
- CLI `xl build` / `xl run` 的主生成 C、生成头和 C 混编 wrapper 已改为挂载动态 VFS 后通过 `tcc_add_file` 编译；默认不再用 `module.c`、`module.h`、`csrc_N.c` 交换中间源码。
- CLI `--save-temps` 是显式调试输出，会把动态 VFS 中的主 C、生成头、XAPI 和 wrapper 导出到工作目录并保留。
- CLI `xl run` 不再输出临时 `app.exe`，也不再创建独立子进程运行脚本；运行参数由 xl.exe 构造为 `array<var>` 后传给 `xlang_main(xvalue arg)`。
- DLL/API 的脚本运行和编译 exe 入口不再为了普通执行预创建 `tempDir`；`tempDir/saveTemps` 保留给后续显式导出动态 VFS 内容。
- `xlRunScriptStringArg` / `xlRunScriptFileArg` 已加入公开头文件和实现。
- `xl.dll` 测试已覆盖新 API 的 `NULL arg` 调用路径。
- 内存运行时 TCC 符号表已补齐当前 generated C 需要的 typed container 与 `xrtDictWalk` 符号。
- 已拆分 `xl.dll` / `xlc.dll` 两个 DLL 产物：`xl.dll` 只导出运行 API，`xlc.dll` 额外导出编译 exe API。
- `xl.exe` 的 `run` / `build` 主链路已静态编入 `lib/tcc/libtcc.c`，不再依赖外部 `tcc.exe` 完成编译。
- `xl.exe` 已接入清单驱动的内置 VFS 资源包，不再使用 `tcc_xlang` 中粗糙的全量实验资源表。
- `xl.exe` / `xlc.dll` 的内置 VFS 资源已启用逐资源 LZMA 压缩，运行期只携带 LZMA decoder。
- 主回归覆盖外部 TCC include/lib、`libtcc1.a`、`xrt.o` 全部缺失时，`xl.exe run` 与 `xl.exe build` 仍可依靠内置 VFS 完成脚本运行和 exe 输出；构建阶段所需 `libtcc1.a`、TCC 头文件/def、XRT 单头文件和 LZMA 源码均必须位于 `demo6` 目录内。
- DLL 回归覆盖 `release\libtcc.dll` 不存在，且外部 TCC include/lib、`libtcc1.a`、`xrt.o` 全部缺失时，`xl.dll` 仍可依靠静态 libtcc 与运行版 VFS 完成脚本运行，`xlc.dll` 仍可依靠静态 libtcc、编译版 VFS 与 `xrt.h` 单头文件完成脚本运行和 exe 输出。
- `lib/tcc` 已加入轻量 UTF-8 IO 适配层，Windows 下普通 `open` / `fopen` / `unlink` 会转为宽字符 API，主回归覆盖中文路径脚本编译到中文路径 exe。
- `xl.exe --loadlib` 需要的 DLL 导出 def 生成已改为内嵌调用 `tcc_tool_impdef`，主回归覆盖外部 `tcc.exe` 缺失时的 `--loadlib` 运行。

## 后续未完成

- 继续压实 `lib/tcc/win32/dist/x86_64` 的来源记录，后续同步外部 TCC 时保持补丁边界清晰。
- 正式设计并实现 XRT/xvalue 从 `xl.dll` 对 C 宿主公开的 ABI 导出策略。
