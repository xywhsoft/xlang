# P2 CLI 与 DLL/API 验证矩阵

本文继续细化 `34_规则测试范例交叉索引.md`，覆盖以下 P2 规则文档：

- `32_P2_CLI构建运行与产物路径细则.md`：35 条
- `33_P2_DLL_API动态执行细则.md`：28 条

合计 63 条规则。本文件不新增 `CGEN-*` 规则，只记录 CLI 构建运行、产物路径、TCC 链接、C 混编 wrapper、`xl.dll` 公共 API、动态执行和编译到 exe 当前由哪些测试、范例和脚本证明。

## CLI 命令与基础产物

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-CLI-001` 默认 TCC backend | `build.bat`、`test.bat` 中大量 `release\xl.exe build/run`、`cli_link.xl`、`cli_loadlib.xl` | build/run 回归 | 默认 TCC 路径、include/lib 前缀、XRT/TCC 运行库路径在完整回归中被反复使用；属于强间接覆盖。 |
| `CGEN-CLI-002..003` 命令帮助、公开子命令、通用选项读取 | `test.bat` 覆盖 `lex/parse/check/ir/cgen/abi/ABI 元数据/run/build`，并检查 `xl help` 输出包含 build 用法；同时使用 `--emit`、`--emit-header`、`--dump-dir`、`-o`、`--json` | CLI 行为覆盖 | 九类命令入口、帮助文本和主要带值选项均有实际调用。 |
| `CGEN-CLI-004..005` header include guard、dump 目录创建、stdout/file 输出 | `xl cgen release\examples\hello.xl --dump-dir build\test\dump_hello`；`findstr` 检查 `hello.h` guard、`hello.c`、tokens/AST/IR/ABI；`xl abi export_add.xl > abi_stdout.json` | 文件产物检查 | dump-dir 全产物、header guard、C/header 文件输出和 stdout ABI 元数据均有直接断言。 |

## CLI Build/Run 与 TCC 链接

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-CLI-006` build 输入收集 | `xl build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c -o ...` | C 混编运行 | 一个 `.xl` 主模块加多个用户 `.c` wrapper 的收集路径有直接运行覆盖；超 32 文件上限仍建议补负例。 |
| `CGEN-CLI-007` 默认链接去重识别 | `cli_link.xl` 使用 `--lib-path build\test --link xl6clitest`；`cgen_main_entry.xl --link %XL6_XRT_OBJ%` | build/run 回归 | 用户库名链接、显式 XRT object 去重、默认 TCC runtime/system 尾链接均有覆盖；默认 XRT object 路径由 `tools\xl6_toolchain_env.bat` 集中提供。 |
| `CGEN-CLI-008` `-shared` 与 `--save-temps` 扫描 | `xl build export_add.xl -shared -o ... --emit-header ...`；`xl build cgen_main_entry.xl -shared` 负例；`build_mixed_save_temps.exe` 检查保留 wrapper | shared build 正/负例 + 临时产物检查 | `-shared` 对 DLL 构建和入口限制有覆盖；`--save-temps` 保留目录和 wrapper 内容已有直接断言。 |
| `CGEN-CLI-009..010` C wrapper include 路径规范化和 wrapper 生成 | `build_mixed_host.c`、`build_mixed_extra.c` 与 `09_c_export_abi.xl` 混编；`--save-temps` 检查 `csrc_0.c/csrc_1.c` | C 宿主运行 + wrapper 文本检查 | wrapper 自动 include XRT、生成头和用户 C 源的能力已有运行与文本双重证明。 |
| `CGEN-CLI-011..012` dump 文件名、临时目录和清理规则 | `dump_hello` 产物检查；`bad_main_shared` 失败前后 `xl6_build_*` 目录计数不变；非法脚本 run 失败前后 `xl6_run_*` 目录计数不变 | 文件产物检查 + 失败清理断言 | dump 文件名和目录拼接有直接断言；build/run 成功和失败路径的默认临时目录清理均有覆盖，`--save-temps` 保留路径另由 wrapper 检查覆盖。 |
| `CGEN-CLI-013..017` run PATH 环境、argv/link 参数、loadlib def、用户/import TCC 参数 | `cli_link.xl`、`cli_loadlib.xl`、import package build/run 用例 | link/loadlib 运行 | 静态库 `--link`、动态库 `--loadlib`、run 阶段 PATH env 注入和 import 依赖传播都有运行覆盖。 |
| `CGEN-CLI-018..019` 默认 XRT/system 库链接和 TCC 参数总装配 | 全部 `xl build/run` 用例，特别是 stdlib、HTTP/process/dir、并发和 C ABI 用例 | 全回归 | XRT object、TCC runtime、Windows 系统库、用户 C 输入、import cSources 进入同一 TCC argv 的路径有持续覆盖。 |

## CLI 调试输出与 Build 入口

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-CLI-020..023` header、受检编译单元、C 输出、ABI 元数据输出命令 | `export_*.xl --emit-header`、`import_metadata.abi.json`、`xl abi export_add.xl > abi_stdout.json`、大量 `xl cgen --emit` | 文件内容检查 | 语义检查后再输出 C/header/ABI 元数据的主路径有强覆盖，ABI stdout 已补快照。 |
| `CGEN-CLI-024..027` lex/parse/check/ir/abi/cgen dump-dir 调试产物 | `test.bat` 的 `dump_hello`、`import_source_use.xlir`、`export_alias.abi.json/txt` | 阶段产物检查 | tokens、AST、IR、ABI、header、C 全阶段 dump 已直接检查关键字段。 |
| `CGEN-CLI-028..030` build 必要参数、shared 入口限制、header/ABI 元数据/C 输出顺序 | `cgen_main_entry.xl` build/run；`bad_main_shared.err.txt`；`export_add.xl -shared --emit-header`；`build_missing_output.err.txt` | build/run + 负例 | `-o` build、缺 `-o` 诊断、ABI 元数据 入口判断、shared 禁止 xlang main、用户 header 输出均有覆盖。 |
| `CGEN-CLI-031..032` build C 输入集合、编译与清理 | `build_mixed_host.exe`、import package build/run、shared DLL 生成、`bad_main_shared` 失败清理计数断言 | C 混编 + package build + 失败清理 | 动态 VFS 主 `__xlvfs__/module.c`、import cSources、动态 VFS 用户 C wrapper 一起编译的路径有覆盖；默认失败清理和 `--save-temps` 导出保留均有直接断言。 |
| `CGEN-CLI-033..035` run 内存执行、程序参数和 CLI 主分派 | `cgen_main_entry.exe alpha beta` 返回 argc 3；`xl_run_args.xl` 通过 `xl run ... -- alpha "two words" "quote\"value" beta ""` 断言 argv；所有命令分派均在 `test.bat` 使用 | run 行为 + 主分派 | `xl run` 通过 TCC 内存重定位直接调用 `xlang_main(xvalue arg)`；`--` 后 argv 传递、含空格/双引号/空字符串参数和 CLI 主分派均有覆盖。 |

## DLL/API 公共接口与 Context

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-API-001..002` 公共 C API 导出形态、上下文选项与公共能力 | `build_dll.bat` 用 `-DXL_BUILD_DLL` 构建 `release\xl.dll`；`release\examples\xl_dll_embed_string.c`、`release\examples\xl_dll_null_context.c` include `xl.h` | DLL 构建 + C 宿主编译 | `XL_API` 导出、`extern "C"` 兼容、`XL_ContextOptions` 和核心 API 原型通过 DLL 与宿主验证程序编译。 |
| `CGEN-API-003..005` 内部上下文状态、错误/list 所有权、宿主 C 符号登记 | `xl_dll_embed_string.c` 调用 `xlCreateContext`、设置 options、`xlRegisterCFunction(ctx, "host_add", ...)`，脚本 extern 调用 `host_add` | C 宿主运行 | context 创建、字符串复制、宿主函数符号登记和 TCC 符号解析由 `hostAdd(20,22)==42` 直接证明。 |
| `CGEN-API-006` 工作目录进入与恢复 | `xl_dll_embed_string.c` 设置 `options.workDir="."`，脚本执行相对路径 `build/test/dll_dir_list`；`xl_dll_null_context.c` 使用 `release\\tmp\\dll_cwd_a` 与 `release\\tmp\\dll_cwd_b` 两个 workDir，分别覆盖成功脚本和解析失败脚本后的 cwd 恢复 | DLL 运行 + C 宿主断言 | 相对路径标准库文件/目录操作成功说明 workDir 入口可用；多 context 成功/失败路径后宿主 cwd 保持不变已有直接断言。 |
| `CGEN-API-018..023` 默认 context options、运行时引用计数、创建/销毁、路径/saveTemps/include/lib/link/loadlib 配置 | `test_dll.bat` 构建并运行 `xl_dll_embed_string.exe` 与 `xl_dll_null_context.exe`；程序调用 `xlSetWorkDir`、`xlSetTempDir`、`xlSetSaveTemps`、`xlAddIncludePath`、`xlAddLibraryPath`、`xlAddLinkLibrary`、`xlLoadDependencyLibrary`，并覆盖这些 setter 的空字符串负例；`xl_dll_embed_string.c` 在外部文件系统放置 `__xlvfs__/module.c` 毒丸文件，验证动态 VFS 中的生成 C 优先于外部同名文件；`xl_dll_null_context.c` 的 `run_dependency_path_snapshot_case` 在相对 `workDir` 下使用相对 library path、裸库名、loadlib DLL 和 def 文件，验证这些依赖不会被脚本工作目录重新解释；NULL context 宿主覆盖所有公开 API 的 NULL 入参、无效 options size/version 和默认 context 创建销毁 | DLL 冒烟 + C 宿主运行 + API 负例 | 默认 options、context 生命周期、XRT runtime init/unit、动态 VFS 优先级、工作目录和 include/lib/link/loadlib setter 正常路径、空值错误路径、路径快照、NULL context 返回失败与空 lastError、无效 options 拒绝均有覆盖。 |
| `CGEN-API-028` lastError 查询与清理 | `xl_dll_embed_string.c` 在非法脚本后检查 `xlGetLastError(ctx)` 含 `__xl_string.xl:1:` 和 `[parse]`，再调用 `xlClearLastError` 确认为空 | 错误通道负例 | lastError 设置、读取、虚拟路径、行列、phase 和清理已有直接断言。 |

## DLL/API TCC 动态执行与编译

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-API-007..008` 静态 libtcc 与 VFS 资源分组 | `build_dll.bat` 不再复制 `libtcc.dll`；DLL/API 测试确认 `release\\libtcc.dll` 不存在也能运行；`xl_dll_embed_string.c` 使用默认静态 libtcc 入口运行字符串/文件脚本并编译 exe；`xl_dll_null_context.c` 覆盖相对 `workDir` 下的路径快照；`check_dll_api_boundary.ps1`、`check_toolchain_paths.ps1` 和 VFS 检查守住静态 libtcc、动态 VFS -> 外部文件系统 -> 静态 VFS、`xl.dll`/`xlc.dll` 资源分组口径 | DLL 运行 + 静态守门 | 无外部 libtcc 依赖、静态 libtcc 入口、VFS 优先级、运行/编译资源分组、路径快照和必需 TCC API 均有覆盖。 |
| `CGEN-API-009..010` 默认 XRT/运行时符号注入、context C 符号注入 | `xl_dll_embed_string.c` 脚本调用 string/json/codec/hash/crypto/id/url/time/regex/process/dir/file 标准库，同时调用 host_add | DLL 运行 + 标准库覆盖 | 内存 TCC 不链接外部 xrt.o 仍能解析 XRT 和 stdlib helper，宿主 C 函数地址也能注入。 |
| `CGEN-API-011..013` TCC state 创建、编译诊断转 lastError、cgen 到内存 | `xlRunScriptString(ctx, script)` 成功；非法脚本检查 lastError 含 `__xl_string.xl:1:` 和 `[parse]`；`tools\check_stdlib_dll_symbols.ps1` 检查 DLL 符号集合 | DLL 运行 + 符号检查 + 负例 | 内存输出、TCC include/lib path、runtime symbols、编译字符串路径和错误通道有覆盖；诊断已检查虚拟路径、行列和阶段。 |
| `CGEN-API-014..016` 编译单元从字符串/文件加载、依赖注入、内存编译并运行 | `xlRunScriptString`、`xlRunScriptFile`；import/std library 脚本运行覆盖依赖和符号注入；`run_dependency_path_snapshot_case` 同时覆盖 bare library name、path-like link file 和 loaded library path 的依赖注入 | DLL 字符串/文件运行 | 字符串脚本和文件脚本入口均有直接测试；context 依赖注入在切换 `workDir` 后仍能解析外部符号。 |
| `CGEN-API-017` 编译到 exe | `xlCompileScriptStringToExe(ctx, buildScript, "release\\tmp\\xl_dll_embed_string_script.exe")`，随后 `test_dll.bat` 运行该 exe；`xlc.dll` 通过单头文件方式编译 `lib/xrt/xrt.h` 并链接 TCC runtime/system libs | DLL 编译到 exe + 产物运行 | 字符串脚本到 exe 的完整路径被直接验证，包括单头文件 XRT 编译、runtime/system libs 链接和输出 exe。 |
| `CGEN-API-024..027` run/compile 字符串与文件顶层 API | `xlRunScriptString`、`xlRunScriptFile`、`xlCompileScriptStringToExe`、`xlCompileScriptFileToExe` 已覆盖；`test_dll.bat` 运行两份生成 exe | 顶层 API 覆盖 | 字符串/文件 run 和字符串/文件 compile-to-exe 均达到可运行烟测。 |

## 剩余风险与后续增强

| 缺口 | 建议补法 |
|---|---|
| CLI 输入上限、`xl run file.xl -- arg`、含空格/引号 argv 已分别由 33 个 `.c` 输入负例和 `xl_run_args.xl` 覆盖。 | 当前 CLI argv 转义路径已补到 RC0 所需强度；后续可增加更多 Windows 特殊字符组合。 |
| DLL API 的 `xlAddLibraryPath`、`xlAddLinkLibrary`、`xlLoadDependencyLibrary`、setter 空字符串负例、NULL context 边界和多上下文 cwd 恢复已由 `xl_dll_embed_string.c` 与 `xl_dll_null_context.c` 覆盖。 | 当前 DLL setter 正常/失败路径、路径快照、NULL context 返回语义、无效 options 拒绝、默认 context 创建销毁、静态 libtcc 入口、成功/失败脚本后的 cwd 恢复均有烟测；后续可扩展到多线程宿主并发调用边界。 |
| DLL lastError 已验证非空、虚拟路径、phase、行列和清理。 | 当前诊断格式锁定强度已满足本阶段；后续可扩展到语义错误与 TCC 链接错误。 |

## 阶段结论

至此，`32` 到 `33` 的 CLI/DLL P2 规则已有矩阵级覆盖说明，并已补强 help/stdout/缺 `-o`、`--save-temps` wrapper、多 C 输入、输入上限、显式 XRT 链接、`xl run --` argv 空格/引号/空字符串传递、DLL 文件入口、DLL link/loadlib setter、依赖路径快照、静态 libtcc 入口、setter 空值负例、NULL context 纯 C 单元测试、多上下文 cwd 恢复和 lastError phase/行列断言。CLI 的主干 build/run/cgen/ABI 元数据/dump/link/loadlib/C 混编路径覆盖较强；DLL 的字符串/文件运行、标准库符号注入、宿主 C 函数注册、外部 link/loadlib、字符串/文件编译到 exe、NULL context 边界、cwd 成功/失败恢复已有可运行验证。
