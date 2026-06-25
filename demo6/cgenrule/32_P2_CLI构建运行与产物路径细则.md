# P2 CLI 构建运行与产物路径细则

本文细化 `demo6/src/driver/xl6_cli.c` 与 `demo6/src/backend/xl6_build_backend.c`。这些规则不一定直接改变 `xl6CgenEmitC` 的输出文本，但会决定 `.xl` 文件何时生成 C、头文件、XAPI、dump 产物、exe/dll，以及如何把 XRT、import package、用户 C 源和动态库链接进最终程序。

## CGEN-CLI-001 默认 TCC backend

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | 默认 backend 固定为 TCC，编译器路径为 `XL6_TCC_EXE_PATH`。命令前缀预置 XRT include、TCC include、WinAPI include、临时目录 include 和 TCC lib 路径。 |
| 生成器文件名和行号 | `demo6/src/backend/xl6_build_backend.c:6` |
| 对应生成的 C 代码 | 不直接生成 C；生成后的 C 会被该 TCC 命令编译。 |
| 中间形态伪代码 | `backend = {name:tcc, ccPath, commandPrefix}` |
| 实现同等功能手写的 C 代码 | 手写构建需要手动拼 `tcc.exe -I... -L...`。 |
| 额外依赖的封装代码 | TCC 可执行文件和 include/lib 目录。 |

## CGEN-CLI-002 命令帮助与公开子命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl help` |
| 生成规则描述 | CLI 公开 lex、parse、check、ir、cgen、abi、xapi、run、build 九类命令。run/build 暴露 `--inc-path`、`--lib-path`、`--link`、`--loadlib`、`--save-temps`，build 额外支持 `-shared`、`--emit-header`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:25` |
| 对应生成的 C 代码 | 不生成 C；定义用户可触发的产物路径集合。 |
| 中间形态伪代码 | `print usage for debug commands and build/run` |
| 实现同等功能手写的 C 代码 | 手写工具链需要维护同等命令帮助。 |
| 额外依赖的封装代码 | CLI main dispatch。 |

## CGEN-CLI-003 通用选项读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--emit out.c`、`--emit-header out.h`、`--dump-dir dump`、`-o app.exe` |
| 生成规则描述 | 文件参数固定取 `argv[2]`；`--json` 或 `--format json` 打开 JSON 输出；`--emit`、`--emit-header`、`--dump-dir`、`-o` 都从命令行中线性扫描第一个后继值。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:39`、`demo6/src/driver/xl6_cli.c:47`、`demo6/src/driver/xl6_cli.c:62`、`demo6/src/driver/xl6_cli.c:98` |
| 对应生成的 C 代码 | 不直接生成 C；影响 cgen/header/xapi/dump 输出位置。 |
| 中间形态伪代码 | `find option value after command/file` |
| 实现同等功能手写的 C 代码 | 手写 CLI 也需要统一解析输出文件选项。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CLI-004 头文件 include guard 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit-header main.h` |
| 生成规则描述 | 默认 guard 为 `XL6_GENERATED_HEADER_H`。若提供路径，则取 base name 去扩展名，字符转大写，非字母数字转 `_`，最终形如 `XL6_MAIN_H`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:110`、`demo6/src/driver/xl6_cli.c:121` |
| 对应生成的 C 代码 | 生成头文件中 `#ifndef XL6_MAIN_H` 形式的 guard。 |
| 中间形态伪代码 | `guard = "XL6_" + sanitizeUpper(baseNoExt) + "_H"` |
| 实现同等功能手写的 C 代码 | 手写头文件也需要 include guard。 |
| 额外依赖的封装代码 | `xl6CgenEmitHeader`。 |

## CGEN-CLI-005 dump 目录创建与输出打开

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump` 或无 `--emit` 输出到 stdout |
| 生成规则描述 | dump 目录必须非空并通过 XRT 递归创建。输出路径为 NULL 时写 stdout，否则写文件；关闭时检查 `XL6_Output` 错误并输出明确诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:165`、`demo6/src/driver/xl6_cli.c:178`、`demo6/src/driver/xl6_cli.c:186` |
| 对应生成的 C 代码 | `--emit` 为空时 C 输出到 stdout；非空时写入文件。 |
| 中间形态伪代码 | `out = path ? openFile(path) : stdout` |
| 实现同等功能手写的 C 代码 | 手写工具要区分 stdout 和文件输出错误。 |
| 额外依赖的封装代码 | `XL6_Output`、XRT 目录 API。 |

## CGEN-CLI-006 build 输入收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe` |
| 生成规则描述 | build 从 `argv[2]` 开始扫描。当前只允许一个 `.xl` 主模块；任意多个 `.c` 作为 C 混编源，最多 32 个；跳过带值选项、`-shared` 和其它 `-` 开头选项。遇到未知非选项输入时报错。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:274` |
| 对应生成的 C 代码 | 主 `.xl` 先生成 `module.c`，用户 `.c` 会被包装为 `csrc_N.c`。 |
| 中间形态伪代码 | `one xl input + up to 32 c inputs` |
| 实现同等功能手写的 C 代码 | 手写混编需要手动维护主模块和 C 源列表。 |
| 额外依赖的封装代码 | C wrapper 生成规则。 |

## CGEN-CLI-007 默认链接去重识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --link xrt` |
| 生成规则描述 | 用户显式 `--link` 到 XRT 或 TCC runtime 时，默认链接不重复追加。XRT 匹配 `xrt`、`xrt.o`、`xrt.obj`、`xrt.lib`、`libxrt.a`；TCC runtime 匹配 `libtcc1-64.a` 或 `libtcc1.a`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:330`、`demo6/src/driver/xl6_cli.c:345`、`demo6/src/driver/xl6_cli.c:357` |
| 对应生成的 C 代码 | 不改变 C 文本；改变 TCC 链接参数。 |
| 中间形态伪代码 | `if user linked xrt: skip default xrt.o` |
| 实现同等功能手写的 C 代码 | 手写构建避免同一对象或库重复进入链接。 |
| 额外依赖的封装代码 | CLI link option parser。 |

## CGEN-CLI-008 `-shared` 与 `--save-temps` 扫描

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -shared --save-temps -o lib.dll` |
| 生成规则描述 | `-shared` 和 `--save-temps` 都在 `--` 前扫描；带值选项会跳过其值。`-shared` 影响 TCC 参数和可执行入口校验；`--save-temps` 影响临时文件是否删除。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:383`、`demo6/src/driver/xl6_cli.c:401` |
| 对应生成的 C 代码 | `-shared` 不改 C 代码，但会定义 `XL6_BUILD_SHARED`。 |
| 中间形态伪代码 | `flags = scanOptionsBeforeDoubleDash()` |
| 实现同等功能手写的 C 代码 | 手写构建同样需要把编译选项和程序参数分隔。 |
| 额外依赖的封装代码 | TCC option append。 |

## CGEN-CLI-009 C wrapper include 路径规范化

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe` |
| 生成规则描述 | Windows 下 wrapper include 路径先 `_fullpath`，再把 `\` 转为 `/`。include 字符串会转义 `"` 和 `\`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:419`、`demo6/src/driver/xl6_cli.c:447` |
| 对应生成的 C 代码 | `#include "D:/.../helper.c"` |
| 中间形态伪代码 | `includePath = fullpath(path).replace("\\","/")` |
| 实现同等功能手写的 C 代码 | 手写混编常需要处理 include 字符串转义。 |
| 额外依赖的封装代码 | C wrapper 生成函数。 |

## CGEN-CLI-010 C 混编 wrapper 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `.xl` 与 `.c` 放在一起 build |
| 生成规则描述 | 每个用户 C 源生成一个 wrapper 文件。wrapper 固定包含 `lib/xrt/xrt.h`、xlang 生成头文件和用户 C 源。这样用户 C 源无需手写 xrt/xlang 头文件 include。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:461` |
| 对应生成的 C 代码 | `#include "lib/xrt/xrt.h"\n#include "module.h"\n#include "helper.c"` |
| 中间形态伪代码 | `wrapper = include(xrt.h) + include(generated.h) + include(user.c)` |
| 实现同等功能手写的 C 代码 | 手写 C 混编需要人工 include 运行库和生成头。 |
| 额外依赖的封装代码 | 生成的 `module.h`。 |

## CGEN-CLI-011 dump 文件名构造

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen src/main.xl --dump-dir dump` |
| 生成规则描述 | 取源文件 base name 去扩展名，和 dumpDir 拼成各产物路径。目录末尾已有分隔符则不重复添加，否则加 `\`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:491`、`demo6/src/driver/xl6_cli.c:505`、`demo6/src/driver/xl6_cli.c:536` |
| 对应生成的 C 代码 | dump 中的 C 文件为 `<dumpDir>/<base>.c`。 |
| 中间形态伪代码 | `dumpPath = dir + sep + base + suffix` |
| 实现同等功能手写的 C 代码 | 手写调试工具也会按源名生成多类产物。 |
| 额外依赖的封装代码 | dump-dir 命令。 |

## CGEN-CLI-012 临时目录与清理规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl` 或 `xl build main.xl -o app.exe` |
| 生成规则描述 | 临时工作目录固定在 `release\tmp\xl6_<purpose>_<time>_<pid>_<counter>`。默认构建结束删除列出的临时文件和工作目录；`--save-temps` 时保留并打印路径。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:552`、`demo6/src/driver/xl6_cli.c:575`、`demo6/src/driver/xl6_cli.c:606` |
| 对应生成的 C 代码 | 临时 C 产物通常是 `module.c`、`module.h`、`module.xapi.json`。 |
| 中间形态伪代码 | `workDir = release/tmp/xl6_build_time_pid_id` |
| 实现同等功能手写的 C 代码 | 手写 build/run 需要避免临时产物互相覆盖。 |
| 额外依赖的封装代码 | XRT 文件/目录删除 API。 |

## CGEN-CLI-013 run 动态库 PATH 前缀

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl --loadlib dep.dll` 或 import 包带 loadlibs |
| 生成规则描述 | run 执行临时 exe 前，会把用户 `--loadlib` 和 importDeps.loadlibs 的目录追加到命令前缀：`set "PATH=dir1;dir2;%PATH%" && app.exe`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:684`、`demo6/src/driver/xl6_cli.c:721` |
| 对应生成的 C 代码 | 不改变生成 C；改变运行时 DLL 搜索路径。 |
| 中间形态伪代码 | `command = set PATH=<loadlib dirs>;%PATH% && exe` |
| 实现同等功能手写的 C 代码 | 手写执行器需要在运行前处理 DLL 搜索路径。 |
| 额外依赖的封装代码 | run command builder。 |

## CGEN-CLI-014 命令文本、路径和链接参数拼接

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--inc-path dir --lib-path dir --link user32` |
| 生成规则描述 | 命令文本追加前检查缓冲区容量；路径和程序实参统一按 Windows C argv 规则带引号并转义内部双引号/反斜杠；`--link` 值若看起来像路径或文件，则按文件直接加入命令，否则转成 `-l<name>`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:753`、`demo6/src/driver/xl6_cli.c:770`、`demo6/src/driver/xl6_cli.c:782`、`demo6/src/driver/xl6_cli.c:797`、`demo6/src/driver/xl6_cli.c:1848` |
| 对应生成的 C 代码 | 不改变 C；生成 TCC 命令参数，如 `-I"dir"`、`-luser32`、`"a.lib"`。 |
| 中间形态伪代码 | `quote(text)=windowsArgvQuote(text); link(value) = looksLikePath ? quote(value) : "-l"+value` |
| 实现同等功能手写的 C 代码 | 手写 TCC 命令必须区分库名和库文件。 |
| 额外依赖的封装代码 | TCC command builder。 |

## CGEN-CLI-015 `--loadlib` import def 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --loadlib a.dll` |
| 生成规则描述 | 每个 loadlib 先调用 TCC `-impdef <dll> -o <def>` 生成 def 文件，再把 def 当链接文件追加到 TCC 命令。def 位于当前 workDir 或 `release\tmp`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:812` |
| 对应生成的 C 代码 | 不改变 C；产生 `xl6_loadlib_N.def` 并链接。 |
| 中间形态伪代码 | `tcc -impdef dll -o def; link(def)` |
| 实现同等功能手写的 C 代码 | 手写 Windows 动态库链接常需要 import library/def。 |
| 额外依赖的封装代码 | TCC `-impdef`。 |

## CGEN-CLI-016 用户 TCC 参数追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--inc-path`、`--lib-path`、`--link`、`--loadlib`、`-shared` |
| 生成规则描述 | `--inc-path` 生成 `-I`；`--lib-path` 生成 `-L`；`--link` 按链接规则追加；`--loadlib` 生成 def 并链接；`-shared` 追加 `-shared -DXL6_BUILD_SHARED`。`--emit`、`--emit-header`、`--dump-dir`、`-o` 在 TCC 参数阶段只跳过值。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:867` |
| 对应生成的 C 代码 | `-DXL6_BUILD_SHARED` 可影响生成/包含代码中的 DLL 条件分支。 |
| 中间形态伪代码 | `scan cli options -> append tcc flags` |
| 实现同等功能手写的 C 代码 | 手写 build 系统需要把用户选项转换为编译器参数。 |
| 额外依赖的封装代码 | loadlib def generator。 |

## CGEN-CLI-017 import 依赖 TCC 参数追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg`，pkg manifest 包含 `links` 或 `loadlibs` |
| 生成规则描述 | importDeps.links 逐项按普通链接规则加入。importDeps.loadlibs 逐项生成 def 并链接，和用户 `--loadlib` 共用计数，最多 32 项。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:934` |
| 对应生成的 C 代码 | 不改变生成 C；改变最终 TCC 链接命令。 |
| 中间形态伪代码 | `for link in deps.links: link(link); for dll in deps.loadlibs: impdef+link` |
| 实现同等功能手写的 C 代码 | 手写包系统需要把包依赖传播到链接器。 |
| 额外依赖的封装代码 | importDeps 收集、loadlib def generator。 |

## CGEN-CLI-018 默认 XRT 与系统库链接

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | TCC 命令总是追加 `-DXL6_USE_XRT_LIFECYCLE`。若用户未显式链接 XRT，则必须找到 `..\..\xrt\release\x64\xrt.o` 并链接；若用户未显式链接 TCC runtime 且存在 `libtcc1-64.a`，则追加它。最后追加 `Ws2_32`、`IPHLPAPI`、`Shell32`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:978` |
| 对应生成的 C 代码 | `-DXL6_USE_XRT_LIFECYCLE` 控制生成程序使用 XRT 生命周期。 |
| 中间形态伪代码 | `add XRT lifecycle define; link xrt.o; link system libs` |
| 实现同等功能手写的 C 代码 | 手写构建需显式链接 XRT 和 Windows 网络/系统库。 |
| 额外依赖的封装代码 | XRT object、TCC runtime、Windows libs。 |

## CGEN-CLI-019 TCC 编译命令总装配

| 项目 | 内容 |
|---|---|
| xlang 写法 | build/run 最终编译 |
| 生成规则描述 | 从 backend commandPrefix 开始，依次追加所有 C 输入、`-o <exe>`、用户 TCC 参数、importDeps TCC 参数，最后追加默认 XRT/TCC runtime/system 链接并执行 `system(command)`。默认链接放在尾部，确保用户显式 `--link xrt.o` 时仍位于 TCC runtime 之前。无 C 输入或命令过长直接失败。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1004`、`demo6/src/driver/xl6_cli.c:1067` |
| 对应生成的 C 代码 | 输入 C 包括生成的 `module.c`、import cSources、用户 C wrapper。 |
| 中间形态伪代码 | `tcc <c inputs> -o exe <user opts> <import opts> <default xrt/runtime/system links>` |
| 实现同等功能手写的 C 代码 | 手写构建需要完整命令行。 |
| 额外依赖的封装代码 | backend、link option、importDeps。 |

## CGEN-CLI-020 头文件输出命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit-header main.h` |
| 生成规则描述 | 头文件输出会打开目标文件，生成 include guard，然后调用 `xl6CgenEmitHeader(root, guard)`。失败时打印 cgen 诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1080`、`demo6/src/driver/xl6_cli.c:1134`、`demo6/src/driver/xl6_cli.c:1190` |
| 对应生成的 C 代码 | 生成 `.h` 声明，不生成 `.c`。 |
| 中间形态伪代码 | `header = cgenHeader(root, guard)` |
| 实现同等功能手写的 C 代码 | 手写 C 混编需要维护导出头。 |
| 额外依赖的封装代码 | cgen header 规则。 |

## CGEN-CLI-021 受检编译单元加载

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任何需要 sem/import 的命令 |
| 生成规则描述 | `xl6CliLoadCheckedUnit` 初始化编译单元，并调用 `xl6CompileUnitLoadFile(path, 1)`。失败时按当前 phase 打印诊断。cgen/build/run/xapi/header/ir/abi 都依赖这一路径。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1103` |
| 对应生成的 C 代码 | 只有加载成功后才进入 cgen。 |
| 中间形态伪代码 | `parse + import expand + sem check` |
| 实现同等功能手写的 C 代码 | 手写编译器同样需要在生成前完成语义检查。 |
| 额外依赖的封装代码 | compile unit pipeline。 |

## CGEN-CLI-022 C 输出文件生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit main.c` |
| 生成规则描述 | 打开 C 输出文件后调用 `xl6CgenEmitC(root)`。输出关闭失败或 cgen 返回失败都会导致命令失败；cgen 失败打印诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1113`、`demo6/src/driver/xl6_cli.c:1207` |
| 对应生成的 C 代码 | 完整 `.c` 翻译单元。 |
| 中间形态伪代码 | `emitC(unit.root) -> cPath` |
| 实现同等功能手写的 C 代码 | 手写编译器需要将 AST 后端输出到文件。 |
| 额外依赖的封装代码 | cgen module 规则。 |

## CGEN-CLI-023 XAPI 输出命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl xapi main.xl --emit main.xapi.json` |
| 生成规则描述 | 先构建 `XL6_XapiModule`，再写入 JSON。调用者可传 outModule 复用构建出的 module，用于 build 阶段判断是否有可执行入口。emitPath 为空时写 stdout。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1145`、`demo6/src/driver/xl6_cli.c:1415` |
| 对应生成的 C 代码 | 不生成 C；生成 `xlang-xapi-1` JSON。 |
| 中间形态伪代码 | `module = buildXapi(root); dumpJson(module)` |
| 实现同等功能手写的 C 代码 | 手写扩展库需要导出接口元数据。 |
| 额外依赖的封装代码 | XAPI dump 规则。 |

## CGEN-CLI-024 parse/check/lex/ir 调试命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl lex`、`xl parse`、`xl check`、`xl ir` |
| 生成规则描述 | lex 直接 dump token；parse 可选语义检查后 dump AST text/json 或输出 `ok`；check 加载完整 compile unit 并输出 text/json 诊断；ir 需要受检编译单元并输出 text/json IR。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1221`、`demo6/src/driver/xl6_cli.c:1267`、`demo6/src/driver/xl6_cli.c:1290`、`demo6/src/driver/xl6_cli.c:1306` |
| 对应生成的 C 代码 | 不生成 C；用于对照 lexer/parser/sem/IR。 |
| 中间形态伪代码 | `debug command -> dump selected stage` |
| 实现同等功能手写的 C 代码 | 手写编译器常保留阶段性 dump 命令。 |
| 额外依赖的封装代码 | lexer/parser/sem/ir dump。 |

## CGEN-CLI-025 cgen 命令与 dump-dir 联动

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump --emit main.c --emit-header main.h` |
| 生成规则描述 | 若指定 dumpDir，先执行完整 dump-dir 命令。若没有 emitPath 和 emitHeaderPath，则只做 dump 后返回；否则继续加载受检单元，生成 C 到 emitPath，并可选生成 header。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1338` |
| 对应生成的 C 代码 | `--emit` 产生 C；`--dump-dir` 也会额外产生 `<base>.c`。 |
| 中间形态伪代码 | `dump?; if emit requested: emitC/header` |
| 实现同等功能手写的 C 代码 | 手写工具链可同时输出调试产物和指定目标产物。 |
| 额外依赖的封装代码 | dump-dir command、cgen/header。 |

## CGEN-CLI-026 ABI dump 命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl abi main.xl --json` |
| 生成规则描述 | 加载受检编译单元后，根据 jsonOutput 输出 ABI text 或 JSON。emitPath 为空时写 stdout。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1385` |
| 对应生成的 C 代码 | 不生成 C；输出 ABI 调试信息。 |
| 中间形态伪代码 | `dumpAbi(root, json?)` |
| 实现同等功能手写的 C 代码 | 手写 ABI 检查通常需要类似 dump。 |
| 额外依赖的封装代码 | ABI dump 模块。 |

## CGEN-CLI-027 dump-dir 全量产物

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump` |
| 生成规则描述 | dump-dir 会创建 7 个产物：`<base>.tokens.json`、`<base>.ast.json`、`<base>.xlir`、`<base>.abi.json`、`<base>.xapi.json`、`<base>.h`、`<base>.c`。各阶段按顺序生成，任何一步失败即停止。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1429` |
| 对应生成的 C 代码 | dump 目录中的 `<base>.c` 为完整 cgen 输出，`<base>.h` 为导出头。 |
| 中间形态伪代码 | `tokens -> checked unit -> ast -> ir -> abi -> xapi -> header -> c` |
| 实现同等功能手写的 C 代码 | 手写编译器调试包通常也输出全阶段快照。 |
| 额外依赖的封装代码 | lexer、compile unit、IR、ABI、XAPI、cgen。 |

## CGEN-CLI-028 build 必要参数与工作文件

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | build 必须提供 `-o`。收集输入后创建 `build` 工作目录，并固定生成 `module.c`、`module.h`、`module.xapi.json` 三个临时产物。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1517`、`demo6/src/driver/xl6_cli.c:1538`、`demo6/src/driver/xl6_cli.c:1547` |
| 对应生成的 C 代码 | 主模块临时 C 文件为 `<workDir>\module.c`。 |
| 中间形态伪代码 | `workDir/build -> module.c/h/xapi` |
| 实现同等功能手写的 C 代码 | 手写构建需要中间 C、头文件和接口元数据。 |
| 额外依赖的封装代码 | workDir/temp cleanup。 |

## CGEN-CLI-029 shared build 入口限制

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build lib.xl -shared -o lib.dll` |
| 生成规则描述 | build 先生成 XAPI module。若指定 `-shared` 且 XAPI 判断模块含 xlang main 或顶层可执行语句，则报错：shared build 不能包含可执行入口。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1565`、`demo6/src/driver/xl6_cli.c:1572` |
| 对应生成的 C 代码 | 不输出最终 DLL；避免把 exe 入口打进共享库目标。 |
| 中间形态伪代码 | `if shared && hasExecutableEntry: error` |
| 实现同等功能手写的 C 代码 | 手写 DLL 构建通常禁止 main。 |
| 额外依赖的封装代码 | XAPI entry metadata。 |

## CGEN-CLI-030 build 导出头与 C 输出顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --emit-header public.h` |
| 生成规则描述 | build 总是先为 wrapper 生成临时 `module.h`，再生成临时 `module.xapi.json`；若用户提供 `--emit-header`，额外生成用户指定头文件；最后生成 `module.c`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1559`、`demo6/src/driver/xl6_cli.c:1578`、`demo6/src/driver/xl6_cli.c:1586` |
| 对应生成的 C 代码 | `module.c` 可 include/配合 `module.h` 与用户 C wrapper 编译。 |
| 中间形态伪代码 | `header(temp); xapi(temp); header(user?); cgen(temp)` |
| 实现同等功能手写的 C 代码 | 手写混编应先产出 C 可见声明再编译用户 C。 |
| 额外依赖的封装代码 | cgen header、XAPI dump。 |

## CGEN-CLI-031 build C 输入集合

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe`，并 import 包带 cSources |
| 生成规则描述 | TCC 输入先加入主 `module.c`，再加入 importDeps.cSources，最后为每个用户 C 源生成 wrapper 并加入。总数受 `cInputs` 固定数组限制，超限报错。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1593`、`demo6/src/driver/xl6_cli.c:1604` |
| 对应生成的 C 代码 | TCC 编译 `module.c`、包 C 源、`csrc_N.c` wrapper。 |
| 中间形态伪代码 | `inputs = [module.c] + importCSources + wrappers(userC)` |
| 实现同等功能手写的 C 代码 | 手写构建需把包源和用户源一起交给 C 编译器。 |
| 额外依赖的封装代码 | importDeps、C wrapper。 |

## CGEN-CLI-032 build 编译与临时文件清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | build 调用 TCC 编译所有 C 输入到 outputPath。成功后释放 XAPI/compile unit，并清理 module.c、module.h、module.xapi.json、用户 C wrapper、loadlib def；`--save-temps` 时保留。失败时打印 `tcc build failed`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1629`、`demo6/src/driver/xl6_cli.c:1639` |
| 对应生成的 C 代码 | 临时 C 和 header 默认不保留，最终产物是 `-o` 指定的 exe/dll。 |
| 中间形态伪代码 | `compile(inputs -> output); cleanup temps` |
| 实现同等功能手写的 C 代码 | 手写构建要处理失败诊断和临时产物。 |
| 额外依赖的封装代码 | TCC compile command。 |

## CGEN-CLI-033 run 编译临时 exe

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl` |
| 生成规则描述 | run 创建 `run` 工作目录，生成 `module.c` 和 `app.exe`。加载受检单元后只生成 C，不生成 header/xapi；TCC 输入为 module.c 加 importDeps.cSources，输出临时 app.exe。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1657`、`demo6/src/driver/xl6_cli.c:1676`、`demo6/src/driver/xl6_cli.c:1686`、`demo6/src/driver/xl6_cli.c:1691` |
| 对应生成的 C 代码 | 临时 `<workDir>\module.c`。 |
| 中间形态伪代码 | `run: cgen -> tcc -> temp exe` |
| 实现同等功能手写的 C 代码 | 手写脚本运行器也常先 JIT/编译到临时程序。 |
| 额外依赖的封装代码 | TCC compile command、importDeps。 |

## CGEN-CLI-034 run 程序参数与执行命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl -- arg1 arg2` |
| 生成规则描述 | run 执行命令由可选 PATH 前缀、`call`、临时 exe 路径、`--` 后的程序参数组成。exe 和每个程序参数单独加引号，`call` 用于避免 Windows `cmd /c` 吞掉以引号开头的命令首尾引号。运行返回非 0 则命令失败。成功或失败后按 saveTemps 规则清理 module.c、app.exe 和 loadlib def。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1709`、`demo6/src/driver/xl6_cli.c:1720`、`demo6/src/driver/xl6_cli.c:1755` |
| 对应生成的 C 代码 | 不改变 C；影响临时 exe 的 argv。 |
| 中间形态伪代码 | `system(PATH_PREFIX + "call " + quotedExe + quotedArgs)` |
| 实现同等功能手写的 C 代码 | 手写脚本运行器需要把宿主 CLI 参数传递给目标程序。 |
| 额外依赖的封装代码 | run command builder。 |

## CGEN-CLI-035 CLI 主分派

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl <cmd> ...` |
| 生成规则描述 | `xl6CliMain` 处理 help 和缺失文件参数后，按命令名分派到 lex、parse、check、ir、cgen、run、build、abi、xapi。未知命令返回错误。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1772` |
| 对应生成的 C 代码 | 不直接生成 C；决定用户请求进入哪条产物路径。 |
| 中间形态伪代码 | `switch argv[1] -> command handler` |
| 实现同等功能手写的 C 代码 | 手写编译器 CLI 也需要集中分派。 |
| 额外依赖的封装代码 | 各命令 handler。 |
