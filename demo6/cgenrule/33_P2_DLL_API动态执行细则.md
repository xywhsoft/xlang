# P2 DLL/API 动态执行细则

本文细化 `demo6/src/api/xl_api.c` 与 `demo6/release/include/xl.h`。这条链路用于 `xl.dll`/嵌入式场景：创建语言上下文、运行字符串或文件脚本、编译脚本为 exe、注册宿主 C 函数，并把当前进程内的 XRT 和运行时符号注入 TCC。

## CGEN-API-001 公共 C API 导出形态

| 项目 | 内容 |
|---|---|
| xlang 写法 | 宿主 C 程序包含 `xl.h` |
| 生成规则描述 | Windows 下定义 `XL_BUILD_DLL` 时 `XL_API` 为 `__declspec(dllexport)`，否则为 `__declspec(dllimport)`；非 Windows 为空。公共 API 使用 `extern "C"` 包裹，版本号为 `XL_API_VERSION 1u`。 |
| 生成器文件名和行号 | `demo6/release/include/xl.h:1` |
| 对应生成的 C 代码 | 不由 cgen 生成；DLL 导出函数遵守 C ABI。 |
| 中间形态伪代码 | `XL_API = dllexport/dllimport/empty` |
| 实现同等功能手写的 C 代码 | 手写 DLL API 需要同样区分导出和导入。 |
| 额外依赖的封装代码 | `include/xl.h`。 |

## CGEN-API-002 上下文选项与公共能力

| 项目 | 内容 |
|---|---|
| xlang 写法 | `XL_ContextOptions options = xlDefaultContextOptions();` |
| 生成规则描述 | API 暴露 context 创建/销毁、工作目录、临时目录、保存临时文件、include/lib/link/loadlib、C 函数注册、运行脚本字符串/文件、编译脚本字符串/文件到 exe，以及 lastError 读取/清理。 |
| 生成器文件名和行号 | `demo6/release/include/xl.h:18` |
| 对应生成的 C 代码 | 宿主 C 直接调用 `xlRunScriptString(ctx, source)` 等 API。 |
| 中间形态伪代码 | `context options + execution/compile APIs` |
| 实现同等功能手写的 C 代码 | 手写嵌入式语言 API 需要同样的上下文对象和错误通道。 |
| 额外依赖的封装代码 | `XL_Context` opaque struct。 |

## CGEN-API-003 内部上下文状态

| 项目 | 内容 |
|---|---|
| xlang 写法 | 通过 API 配置上下文 |
| 生成规则描述 | `XL_Context` 保存 baseDir、workDir、tempDir、TCC root/include/lib 路径快照、保留的 tccDllPath 字段快照、saveTemps、lastError、includePaths、libraryPaths、linkLibraries、loadedLibraries 和注册的 C 符号表。当前发布构建静态链接 libtcc，tccDllPath 不参与 TCC 加载。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:62`、`demo6/src/api/xl_api.c:81`、`demo6/src/api/xl_api.c:439` |
| 对应生成的 C 代码 | 不直接生成 C；这些字段决定内存编译和 exe 编译参数。 |
| 中间形态伪代码 | `ctx = {baseDir, paths, tccSnapshots, links, loadedLibHandles, symbols, lastError}` |
| 实现同等功能手写的 C 代码 | 手写嵌入式上下文需要隔离不同宿主配置。 |
| 额外依赖的封装代码 | XRT 内存 API。 |

## CGEN-API-004 错误文本与列表所有权

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlGetLastError(ctx)` |
| 生成规则描述 | 错误文本由 context 持有，设置新错误前释放旧错误。字符串列表追加时拒绝空字符串，复制文本并扩容；释放时逐项释放并清零。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:217`、`demo6/src/api/xl_api.c:226`、`demo6/src/api/xl_api.c:323`、`demo6/src/api/xl_api.c:337` |
| 对应生成的 C 代码 | 不直接生成 C；影响 API 错误报告和路径列表生命周期。 |
| 中间形态伪代码 | `ctx.lastError = strdup(message); list += strdup(text)` |
| 实现同等功能手写的 C 代码 | 手写 DLL API 应避免把临时错误字符串暴露给用户。 |
| 额外依赖的封装代码 | XRT copy/realloc/free。 |

## CGEN-API-005 宿主 C 符号登记

| 项目 | 内容 |
|---|---|
| xlang 写法 | 宿主调用 `xlRegisterCFunction(ctx, "host_print", fn)` |
| 生成规则描述 | 符号名和函数指针必须非空。若同名符号已存在，则更新 value；否则扩容符号表并复制符号名。销毁 context 时释放所有符号名和数组。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:360`、`demo6/src/api/xl_api.c:1101`、`demo6/src/api/xl_api.c:1611` |
| 对应生成的 C 代码 | xlang 生成的 C 中引用该 extern symbol 时，TCC 在内存执行阶段可解析到宿主函数地址。 |
| 中间形态伪代码 | `symbols[name] = fn` |
| 实现同等功能手写的 C 代码 | 手写嵌入式 JIT 通常也提供 add_symbol。 |
| 额外依赖的封装代码 | TCC symbol injection。 |

## CGEN-API-006 工作目录进入与恢复

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlSetWorkDir(ctx, "project")` 后运行脚本 |
| 生成规则描述 | 每次 run/compile 前进入 context workDir，并在结束后恢复旧目录。Windows 使用 UTF-8 到 UTF-16 转换和 `SetCurrentDirectoryW`，非 Windows 使用 `getcwd/chdir`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:237`、`demo6/src/api/xl_api.c:467` |
| 对应生成的 C 代码 | 不改变生成 C；影响 import、相对路径和 TCC 文件查找。 |
| 中间形态伪代码 | `old = cwd; chdir(ctx.workDir); finally chdir(old)` |
| 实现同等功能手写的 C 代码 | 宿主调用脚本时常需要隔离当前工作目录。 |
| 额外依赖的封装代码 | XRT UTF-8/UTF-16 转换。 |

## CGEN-API-007 静态 libtcc 入口

| 项目 | 内容 |
|---|---|
| xlang 写法 | 第一次运行或编译脚本 |
| 生成规则描述 | API 直接使用当前 `xl.exe` / `xl.dll` / `xlc.dll` 内静态链接的 libtcc 入口，不再动态加载外部 `libtcc.dll`。必须能访问 tcc_new/delete/set_lib_path/set_error_func/set_options/add_include_path/add_library_path/add_library/add_file/add_symbol/set_output_type/compile_string/output_file/run 等符号。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:650`、`demo6/src/api/xl_api.c:714` |
| 对应生成的 C 代码 | 不改变生成 C；决定 TCC 是否可用。 |
| 中间形态伪代码 | `use statically linked libtcc procs` |
| 实现同等功能手写的 C 代码 | 手写嵌入式 TCC 可以直接静态链接 libtcc 并调用其公开 API。 |
| 额外依赖的封装代码 | 静态 libtcc API。 |

## CGEN-API-008 DLL 运行/编译资源分组

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl.dll` 运行脚本，`xlc.dll` 运行并编译 exe |
| 生成规则描述 | `xl.dll` 只携带运行脚本所需的 VFS 清单；`xlc.dll` 在运行清单基础上携带编译 exe 所需的 TCC runtime、系统 import 库、头文件等 VFS/LZMA 资源。文件查找统一遵循动态 VFS -> 外部文件系统 -> 静态 VFS，且不依赖外部 `libtcc.dll`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:714`、`demo6/src/api/xl_api.c:732` |
| 对应生成的 C 代码 | 不改变生成 C；决定 TCC include/library/resource 查找能否自洽。 |
| 中间形态伪代码 | `open(path): dynamicVfs(path) || filesystem(path) || staticVfs(path)` |
| 实现同等功能手写的 C 代码 | 手写嵌入式 API 可把运行资源和编译资源分组打包，并用外部文件覆盖内置兜底资源。 |
| 额外依赖的封装代码 | 静态 libtcc、内置 VFS、LZMA 解压。 |

## CGEN-API-009 默认 XRT/运行时符号注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptString(ctx, "print(1)")` |
| 生成规则描述 | 内存执行时，API 把当前进程里的 XRT、xvalue、typed container、callable、并发、HTTP server 等符号逐个 `tcc_add_symbol` 到 TCC。还注入负零常量 `__mzerosf`、`__mzerodf`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:761`、`demo6/src/api/xl_api.c:773` |
| 对应生成的 C 代码 | 生成 C 可以直接引用 `xrtMalloc`、`xvoCreateInt`、`xrtTypedArrayCreate` 等符号，无需链接外部 xrt.o。 |
| 中间形态伪代码 | `for each required runtime symbol: tcc_add_symbol(name, address)` |
| 实现同等功能手写的 C 代码 | 手写 JIT 宿主需要把运行库函数地址注入编译器。 |
| 额外依赖的封装代码 | 当前进程内 XRT。 |

## CGEN-API-010 context C 符号注入

| 项目 | 内容 |
|---|---|
| xlang 写法 | `extern "host_print" func print(int x)` 后宿主注册 `host_print` |
| 生成规则描述 | 创建内存 TCC state 时，先注入默认 XRT 符号，再遍历 context 注册符号。任一符号注入失败都会设置错误并终止编译。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1101` |
| 对应生成的 C 代码 | 生成 C 的 extern symbol 可在 TCC 内存运行时解析。 |
| 中间形态伪代码 | `addDefaultRuntimeSymbols(); for ctx.symbols addSymbol()` |
| 实现同等功能手写的 C 代码 | 手写宿主函数桥接需要把 C 函数地址注册给 JIT。 |
| 额外依赖的封装代码 | `xlRegisterCFunction`。 |

## CGEN-API-011 TCC state 创建

| 项目 | 内容 |
|---|---|
| xlang 写法 | run/compile API 内部创建 TCC state |
| 生成规则描述 | 创建 TCC state 后设置错误回调和 lib 路径，设置输出类型，设置 C 选项 `-std=c11 -D_CRT_SECURE_NO_WARNINGS`，添加默认 include/library 路径和可选 libtcc runtime。TCC lib root 优先使用 context tccLibPath，其次使用 `XL6_TCC_ROOT` 环境变量，最后回退 `XL6_TCC_ROOT_PATH`。TCC include、Win include、WinAPI include、library path、libtcc runtime 分别优先读取 `XL6_TCC_INCLUDE`、`XL6_TCC_WIN_INCLUDE`、`XL6_TCC_WINAPI_INCLUDE`、`XL6_TCC_LIB`、`XL6_TCC_LIBTCC1`，未设置时回退源码级默认值；这些路径在 context 创建时按当时 cwd 做快照，避免进入 workDir 后被重新解释。然后追加 context include/library 路径；内存输出时注入默认和宿主符号。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1118` |
| 对应生成的 C 代码 | 生成 C 会在该 TCC state 内编译。 |
| 中间形态伪代码 | `new TCC; set output; add paths; if memory add symbols` |
| 实现同等功能手写的 C 代码 | 手写嵌入式 TCC 需要初始化 include/lib/search path。 |
| 额外依赖的封装代码 | libtcc API。 |

## CGEN-API-012 编译诊断转 lastError

| 项目 | 内容 |
|---|---|
| xlang 写法 | 脚本语法或语义错误 |
| 生成规则描述 | 若 compile unit 有诊断，取第一条诊断，格式化为 `path:line:column: [phase] message` 写入 lastError；否则写入 `xlang compile failed`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1200` |
| 对应生成的 C 代码 | 不生成 C；用于 API 错误报告。 |
| 中间形态伪代码 | `lastError = firstDiag || generic compile failed` |
| 实现同等功能手写的 C 代码 | 嵌入式编译 API 需要把诊断转为宿主可读错误。 |
| 额外依赖的封装代码 | compile unit diagnostics。 |

## CGEN-API-013 cgen 到内存

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptString` 或 `xlCompileScriptFileToExe` |
| 生成规则描述 | 通过 `xl6OutputOpenMemory` 创建内存输出，调用 `xl6CgenEmitC`，成功后 `xl6OutputTakeMemoryText` 取得完整 C 源文本。输出失败、cgen 失败或取文本失败都会写 lastError。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1224` |
| 对应生成的 C 代码 | 完整 C 源存在内存字符串 `cText` 中。 |
| 中间形态伪代码 | `cText = emitC(root)` |
| 实现同等功能手写的 C 代码 | JIT 场景通常把生成 C 保存在内存而不是文件。 |
| 额外依赖的封装代码 | `XL6_Output` memory backend。 |

## CGEN-API-014 编译单元从字符串或文件加载

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptString(ctx, source)` 或 `xlRunScriptFile(ctx, path)` |
| 生成规则描述 | 字符串 source 不能为空，虚拟路径固定为 `__xl_string.xl`；文件 path 不能为空。两者都调用 compile unit 加载并启用语义检查，失败转 lastError。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1254`、`demo6/src/api/xl_api.c:1268` |
| 对应生成的 C 代码 | 加载成功后才进入 cgen 到内存。 |
| 中间形态伪代码 | `loadSource(source,"__xl_string.xl",check) | loadFile(path,check)` |
| 实现同等功能手写的 C 代码 | 嵌入式脚本语言通常同时支持字符串和文件入口。 |
| 额外依赖的封装代码 | compile unit pipeline。 |

## CGEN-API-015 TCC 依赖注入规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | context 手动添加 link/loadlib，或 import 包带 links/loadlibs |
| 生成规则描述 | context linkLibraries 中像路径的值在 `xlAddLinkLibrary` 调用时已做路径快照，依赖注入时用 `tcc_add_file`；裸库名保留为库名并用 `tcc_add_library`。context loadedLibraries 保存 `xlLoadDependencyLibrary` 加载时的快照路径，全部用 `tcc_add_file`。importDeps.links 先尝试 library，再尝试 file；importDeps.loadlibs 用 `tcc_add_file`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1282` |
| 对应生成的 C 代码 | 不改变 C；决定 TCC 内存或 exe 编译时如何解析外部符号。 |
| 中间形态伪代码 | `link path? add_file : add_library; import links fallback library->file` |
| 实现同等功能手写的 C 代码 | 手写编译宿主需要区分库名和库文件。 |
| 额外依赖的封装代码 | TCC add_library/add_file。 |

## CGEN-API-016 内存编译并运行

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptString` 或 `xlRunScriptFile` |
| 生成规则描述 | 先 cgen 到内存，内存运行 option 会抑制 C `main` wrapper，只保留 `xlang_main(xvalue arg)`；再创建 `TCC_OUTPUT_MEMORY` state。importDeps.cSources 作为文件加入 TCC；随后编译 cText；再追加 context/import link/loadlib 依赖，确保 archive 在生成 C 产生未解析符号后再参与解析；最后 `tcc_relocate()` + `tcc_get_symbol("xlang_main")` 并直接调用入口。返回码非 0 时写 `script returned N`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1320`、`demo6/src/api/xl_api.c:1331` |
| 对应生成的 C 代码 | `cText` 在内存中编译执行，不落盘。 |
| 中间形态伪代码 | `emitC(noMainWrapper) -> TCC memory -> add import C sources -> compile string -> add links/loadlibs -> relocate -> entry(arg)` |
| 实现同等功能手写的 C 代码 | 手写 JIT 执行可直接重定位并调用导出的入口函数。 |
| 额外依赖的封装代码 | TCC memory output、runtime symbol injection。 |

## CGEN-API-017 编译到 exe

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlCompileScriptFileToExe(ctx, "main.xl", "app.exe")` |
| 生成规则描述 | outputExe 必须非空。cgen 到内存后创建 `TCC_OUTPUT_EXE` state，追加 `-DXL6_USE_XRT_LIFECYCLE`，先编译负零常量定义，再用单头文件方式编译 XRT：定义 `XRT_IMPLEMENTATION` 并包含 `lib/xrt/xrt.h`。随后编译 cText，加入 TCC runtime、import cSources、context/import 依赖和 Windows 系统库，最后 `tcc_output_file`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1369`、`demo6/src/api/xl_api.c:1384` |
| 对应生成的 C 代码 | 内存中的完整 C 被 TCC 输出为 exe。 |
| 中间形态伪代码 | `emitC -> TCC exe -> compile constants -> compile XRT_IMPLEMENTATION+xrt.h -> compile cText -> link runtime/system -> output` |
| 实现同等功能手写的 C 代码 | 手写编译 API 需要编译运行库、用户代码并链接系统库。 |
| 额外依赖的封装代码 | 单头文件 XRT、TCC runtime、system libs。 |

## CGEN-API-018 默认上下文选项

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlDefaultContextOptions()` |
| 生成规则描述 | 默认 size/version 填当前 API；workDir 为 `.`，tempDir 为 `release\tmp`，保留字段 tccDllPath 为空，tccLibPath 为空，saveTemps 为 0。创建 context 时若 tccLibPath 仍为空，则按 `XL6_TCC_ROOT` 环境变量或 `XL6_TCC_ROOT_PATH` 默认值补齐。当前发布构建静态链接 libtcc，因此 tccDllPath 不作为加载路径使用。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1441` |
| 对应生成的 C 代码 | 不生成 C；影响后续 TCC 和路径解析。 |
| 中间形态伪代码 | `options = defaults(API_VERSION)` |
| 实现同等功能手写的 C 代码 | DLL API 通常提供默认 options 以便 ABI 扩展。 |
| 额外依赖的封装代码 | `XL_ContextOptions`。 |

## CGEN-API-019 运行时引用计数

| 项目 | 内容 |
|---|---|
| xlang 写法 | 多个 context 并存 |
| 生成规则描述 | 第一个 context 创建前调用 `xrtInit`；运行时计数递增。销毁 context 时计数递减，降到 0 才调用 `xrtUnit`。计数小于等于 0 时 `xlRuntimeUnit` 直接返回。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1514`、`demo6/src/api/xl_api.c:1524` |
| 对应生成的 C 代码 | 不生成 C；保证宿主进程内 XRT 生命周期成对。 |
| 中间形态伪代码 | `if count==0 xrtInit(); count++; count--; if count==0 xrtUnit()` |
| 实现同等功能手写的 C 代码 | 嵌入式运行库常用引用计数保护全局初始化。 |
| 额外依赖的封装代码 | XRT lifecycle。 |

## CGEN-API-020 context 创建与 ABI 校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlCreateContext(&options)` |
| 生成规则描述 | 创建 context 前初始化运行时。options 为空则用默认值；若 size 非 0 且小于当前结构体大小，或 version 非 0 且不等于当前 API_VERSION，则失败并回滚运行时计数。成功后记录创建时 cwd 作为 baseDir，并按 baseDir 快照 workDir、tempDir、保留的 tccDllPath 字段和 TCC root/include/lib 路径，最后设置 saveTemps。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1557` |
| 对应生成的 C 代码 | 不生成 C；决定嵌入式上下文是否有效。 |
| 中间形态伪代码 | `validate options size/version; calloc ctx; baseDir=cwd; snapshot paths; saveTemps=bool` |
| 实现同等功能手写的 C 代码 | 稳定 C ABI 常用 size/version 检查。 |
| 额外依赖的封装代码 | XRT allocation。 |

## CGEN-API-021 context 销毁

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlDestroyContext(ctx)` |
| 生成规则描述 | 销毁 context 时释放 baseDir、workDir、tempDir、TCC 路径快照、保留字段快照和 lastError，释放 include/library/link 列表，关闭并释放 loaded library 句柄表，释放符号表，释放 context，并调用 `xlRuntimeUnit`。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1611` |
| 对应生成的 C 代码 | 不生成 C；释放 API 侧所有资源。 |
| 中间形态伪代码 | `free ctx fields; runtimeUnit()` |
| 实现同等功能手写的 C 代码 | 宿主 API 必须给出明确销毁函数。 |
| 额外依赖的封装代码 | context lists/symbols lifecycle。 |

## CGEN-API-022 context 路径与 saveTemps 配置

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlSetWorkDir`、`xlSetTempDir`、`xlSetSaveTemps` |
| 生成规则描述 | workDir/tempDir 必须非空，并在 setter 调用时按宿主当前目录固化为路径快照；空路径和分配失败都会写 lastError；saveTemps 只做布尔归一。run/compile 公开入口进入 workDir 后执行，主生成 C 通过动态 VFS 传给 TCC，不再为了普通执行预创建 tempDir；saveTemps 当前不额外制造落盘产物。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1851`、`demo6/src/api/xl_api.c:1875`、`demo6/src/api/xl_api.c:1899`、`demo6/src/api/xl_api.c:2043`、`demo6/src/api/xl_api.c:2099` |
| 对应生成的 C 代码 | 不生成 C；tempDir 只保存为后续显式导出动态 VFS 或其他落盘产物的根目录配置，workDir 影响后续 chdir。 |
| 中间形态伪代码 | `ctx.tempDir = snapshot(cwd,path); old = cwd; chdir(ctx.workDir); mountDynamicVfs("__xlvfs__/module.c", cText); ctx.saveTemps = enabled!=0` |
| 实现同等功能手写的 C 代码 | 嵌入式语言上下文通常允许宿主指定工作目录。 |
| 额外依赖的封装代码 | cwd scope。 |

## CGEN-API-023 include/library/link/loadlib 配置

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlAddIncludePath`、`xlAddLibraryPath`、`xlAddLinkLibrary`、`xlLoadDependencyLibrary` |
| 生成规则描述 | include/library path 必须非空，并在 API 调用时按宿主当前目录做路径快照后追加到 context 列表。link 仍支持裸库名：裸库名保持原文本，像路径的值按宿主当前目录做路径快照。load dependency 路径必须非空，先快照再立即加载动态库：Windows 用 UTF-16 `LoadLibraryW`，非 Windows 用 `dlopen(..., RTLD_NOW | RTLD_GLOBAL)`；成功后把句柄和同一快照路径记录到 context，销毁 context 时关闭句柄。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1680`、`demo6/src/api/xl_api.c:1699`、`demo6/src/api/xl_api.c:1718`、`demo6/src/api/xl_api.c:1737` |
| 对应生成的 C 代码 | 不生成 C；影响 TCC 编译和动态符号可见性。 |
| 中间形态伪代码 | `ctx.includePaths += snapshot(path); link += bareName || snapshot(pathLike); loadlib: snapshot -> LoadLibrary/dlopen -> remember {snapshot, handle}` |
| 实现同等功能手写的 C 代码 | 宿主 JIT 编译器需要提供 include/lib/link 注入点。 |
| 额外依赖的封装代码 | platform dynamic loader。 |

## CGEN-API-024 运行字符串脚本

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptString(ctx, "print(1)")` |
| 生成规则描述 | 清空 lastError，进入 workDir，用虚拟路径 `__xl_string.xl` 加载字符串并语义检查，成功后内存编译并运行，释放 compile unit，恢复工作目录。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1808` |
| 对应生成的 C 代码 | 字符串脚本生成的 C 只存在于内存。 |
| 中间形态伪代码 | `with workDir: loadSource -> compileAndRun` |
| 实现同等功能手写的 C 代码 | 嵌入式脚本 API 的核心入口。 |
| 额外依赖的封装代码 | cgen to memory、TCC memory run。 |

## CGEN-API-025 运行脚本文件

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlRunScriptFile(ctx, "main.xl")` |
| 生成规则描述 | 清空 lastError，进入 workDir，按真实 path 加载文件并语义检查，成功后内存编译并运行，释放 compile unit，恢复工作目录。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1837` |
| 对应生成的 C 代码 | 文件脚本生成的 C 只存在于内存。 |
| 中间形态伪代码 | `with workDir: loadFile -> compileAndRun` |
| 实现同等功能手写的 C 代码 | 文件脚本运行入口通常和字符串运行共享后端。 |
| 额外依赖的封装代码 | compile unit file loader。 |

## CGEN-API-026 编译字符串脚本到 exe

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlCompileScriptStringToExe(ctx, source, "app.exe")` |
| 生成规则描述 | 清空 lastError，进入 workDir，用 `__xl_string.xl` 加载字符串并语义检查，成功后调用 exe 编译流程，释放 compile unit，恢复工作目录。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1866` |
| 对应生成的 C 代码 | 生成 C 在内存中交给 TCC，最终输出 exe 文件。 |
| 中间形态伪代码 | `with workDir: loadSource -> compileUnitToExe` |
| 实现同等功能手写的 C 代码 | 嵌入式 API 可直接把动态脚本固化为可执行文件。 |
| 额外依赖的封装代码 | TCC exe output。 |

## CGEN-API-027 编译脚本文件到 exe

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlCompileScriptFileToExe(ctx, "main.xl", "app.exe")` |
| 生成规则描述 | 清空 lastError，进入 workDir，加载脚本文件并语义检查，成功后调用 exe 编译流程，释放 compile unit，恢复工作目录。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1895` |
| 对应生成的 C 代码 | 生成 C 在内存中交给 TCC，最终输出 exe 文件。 |
| 中间形态伪代码 | `with workDir: loadFile -> compileUnitToExe` |
| 实现同等功能手写的 C 代码 | 文件到 exe 是命令行 build 的嵌入式 API 版本。 |
| 额外依赖的封装代码 | TCC exe output。 |

## CGEN-API-028 lastError 查询与清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | `const char* err = xlGetLastError(ctx); xlClearLastError(ctx);` |
| 生成规则描述 | lastError 为空或 context 为空时返回空字符串。清理 lastError 会释放旧文本并置空。run/compile 入口在开始时都会清空旧错误。 |
| 生成器文件名和行号 | `demo6/src/api/xl_api.c:1924`、`demo6/src/api/xl_api.c:1932` |
| 对应生成的 C 代码 | 不生成 C；给宿主提供稳定错误读取接口。 |
| 中间形态伪代码 | `return ctx.lastError || ""; clear => free + null` |
| 实现同等功能手写的 C 代码 | C API 通常用 lastError 避免跨 ABI 抛异常。 |
| 额外依赖的封装代码 | context error storage。 |
