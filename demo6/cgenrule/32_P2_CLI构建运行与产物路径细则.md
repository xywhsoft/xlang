# P2 CLI 构建运行与产物路径细则

本文细化 `demo6/src/driver/xl6_cli.c` 与 `demo6/src/backend/xl6_build_backend.c`。这些规则不一定直接改变 `xl6CgenEmitC` 的输出文本，但会决定 `.xl` 文件何时生成 C、头文件、XAPI、dump 产物、exe/dll，以及如何把 XRT、import package、用户 C 源和动态库链接进最终程序。

## CGEN-CLI-001 默认 TCC backend

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | 默认 backend 固定为 TCC，编译器路径优先读取 `XL6_TCC` 环境变量，未设置时回退 `XL6_TCC_EXE_PATH`。TCC 参数前缀预置 XRT include、TCC include、Win include、WinAPI include、临时目录 include 和 TCC lib 路径；TCC include/lib 路径优先读取 `XL6_TCC_INCLUDE`、`XL6_TCC_WIN_INCLUDE`、`XL6_TCC_WINAPI_INCLUDE`、`XL6_TCC_LIB` 环境变量，未设置时回退源码级默认值。 |
| 生成器文件名和行号 | `demo6/src/backend/xl6_build_backend.c:6` |
| 对应生成的 C 代码 | 不直接生成 C；生成后的 C 会被该 TCC argv 编译。 |
| 中间形态伪代码 | `backend = {name:tcc, ccPath, defaultToolchainArgs}` |
| 实现同等功能手写的 C 代码 | 手写构建需要手动构造 `tcc.exe` 的 include/lib 参数。 |
| 额外依赖的封装代码 | TCC 可执行文件和 include/lib 目录。 |

## CGEN-CLI-002 命令帮助与公开子命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl help` |
| 生成规则描述 | CLI 公开 lex、parse、check、ir、cgen、abi、xapi、run、build 九类命令。run/build 暴露 `--inc-path`、`--lib-path`、`--link`、`--loadlib`、`--save-temps`，build 额外支持 `-shared`、`--emit-header`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:26` |
| 对应生成的 C 代码 | 不生成 C；定义用户可触发的产物路径集合。 |
| 中间形态伪代码 | `print usage for debug commands and build/run` |
| 实现同等功能手写的 C 代码 | 手写工具链需要维护同等命令帮助。 |
| 额外依赖的封装代码 | CLI main dispatch。 |

## CGEN-CLI-003 通用选项读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--emit out.c`、`--emit-header out.h`、`--dump-dir dump`、`-o app.exe` |
| 生成规则描述 | 文件参数固定取 `argv[2]`；`--json` 或 `--format json` 打开 JSON 输出；`--emit`、`--emit-header`、`--dump-dir`、`-o` 都从命令行中线性扫描第一个后继值。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:40`、`demo6/src/driver/xl6_cli.c:48`、`demo6/src/driver/xl6_cli.c:63`、`demo6/src/driver/xl6_cli.c:99` |
| 对应生成的 C 代码 | 不直接生成 C；影响 cgen/header/xapi/dump 输出位置。 |
| 中间形态伪代码 | `find option value after command/file` |
| 实现同等功能手写的 C 代码 | 手写 CLI 也需要统一解析输出文件选项。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-CLI-004 头文件 include guard 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit-header main.h` |
| 生成规则描述 | 默认 guard 为 `XL6_GENERATED_HEADER_H`。若提供路径，则取 base name 去扩展名，字符转大写，非字母数字转 `_`，最终形如 `XL6_MAIN_H`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:111`、`demo6/src/driver/xl6_cli.c:122` |
| 对应生成的 C 代码 | 生成头文件中 `#ifndef XL6_MAIN_H` 形式的 guard。 |
| 中间形态伪代码 | `guard = "XL6_" + sanitizeUpper(baseNoExt) + "_H"` |
| 实现同等功能手写的 C 代码 | 手写头文件也需要 include guard。 |
| 额外依赖的封装代码 | `xl6CgenEmitHeader`。 |

## CGEN-CLI-005 dump 目录创建与输出打开

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump` 或无 `--emit` 输出到 stdout |
| 生成规则描述 | dump 目录必须非空并通过 XRT 递归创建。输出路径为 NULL 时写 stdout，否则写文件；关闭时检查 `XL6_Output` 错误并输出明确诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:166`、`demo6/src/driver/xl6_cli.c:179`、`demo6/src/driver/xl6_cli.c:187` |
| 对应生成的 C 代码 | `--emit` 为空时 C 输出到 stdout；非空时写入文件。 |
| 中间形态伪代码 | `out = path ? openFile(path) : stdout` |
| 实现同等功能手写的 C 代码 | 手写工具要区分 stdout 和文件输出错误。 |
| 额外依赖的封装代码 | `XL6_Output`、XRT 目录 API。 |

## CGEN-CLI-006 build 输入收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe` |
| 生成规则描述 | build 从 `argv[2]` 开始扫描。当前只允许一个 `.xl` 主模块；任意多个 `.c` 作为 C 混编源，最多 32 个；跳过带值选项、`-shared` 和其它 `-` 开头选项。遇到未知非选项输入时报错。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:275` |
| 对应生成的 C 代码 | 主 `.xl` 先生成 `module.c`，用户 `.c` 会被包装为 `csrc_N.c`。 |
| 中间形态伪代码 | `one xl input + up to 32 c inputs` |
| 实现同等功能手写的 C 代码 | 手写混编需要手动维护主模块和 C 源列表。 |
| 额外依赖的封装代码 | C wrapper 生成规则。 |

## CGEN-CLI-007 默认链接去重识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --link xrt` |
| 生成规则描述 | 用户显式 `--link` 到 XRT 或 TCC runtime 时，默认链接不重复追加。XRT 按 basename 匹配常见对象/静态库形式；TCC runtime 按 basename 匹配工具链环境提供的 runtime 库名变体，不绑定固定 release 路径。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:331`、`demo6/src/driver/xl6_cli.c:346`、`demo6/src/driver/xl6_cli.c:358` |
| 对应生成的 C 代码 | 不改变 C 文本；改变 TCC 链接参数。 |
| 中间形态伪代码 | `if user linked xrt: skip default xrt.o` |
| 实现同等功能手写的 C 代码 | 手写构建避免同一对象或库重复进入链接。 |
| 额外依赖的封装代码 | CLI link option parser。 |

## CGEN-CLI-008 `-shared` 与 `--save-temps` 扫描

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -shared --save-temps -o lib.dll` |
| 生成规则描述 | `-shared` 和 `--save-temps` 都在 `--` 前扫描；带值选项会跳过其值。`-shared` 影响 TCC 参数和可执行入口校验；`--save-temps` 影响临时文件是否删除。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:384`、`demo6/src/driver/xl6_cli.c:402` |
| 对应生成的 C 代码 | `-shared` 不改 C 代码，但会定义 `XL6_BUILD_SHARED`。 |
| 中间形态伪代码 | `flags = scanOptionsBeforeDoubleDash()` |
| 实现同等功能手写的 C 代码 | 手写构建同样需要把编译选项和程序参数分隔。 |
| 额外依赖的封装代码 | TCC option append。 |

## CGEN-CLI-009 C wrapper include 路径规范化

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe` |
| 生成规则描述 | Windows 下 wrapper include 路径先 `_fullpath`，再把 `\` 转为 `/`。include 字符串会转义 `"` 和 `\`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:420`、`demo6/src/driver/xl6_cli.c:448` |
| 对应生成的 C 代码 | `#include "D:/.../helper.c"` |
| 中间形态伪代码 | `includePath = fullpath(path).replace("\\","/")` |
| 实现同等功能手写的 C 代码 | 手写混编常需要处理 include 字符串转义。 |
| 额外依赖的封装代码 | C wrapper 生成函数。 |

## CGEN-CLI-010 C 混编 wrapper 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `.xl` 与 `.c` 放在一起 build |
| 生成规则描述 | 每个用户 C 源生成一个 wrapper 文件。wrapper 固定包含 `lib/xrt/xrt.h`、xlang 生成头文件和用户 C 源。这样用户 C 源无需手写 xrt/xlang 头文件 include。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:462` |
| 对应生成的 C 代码 | `#include "lib/xrt/xrt.h"\n#include "module.h"\n#include "helper.c"` |
| 中间形态伪代码 | `wrapper = include(xrt.h) + include(generated.h) + include(user.c)` |
| 实现同等功能手写的 C 代码 | 手写 C 混编需要人工 include 运行库和生成头。 |
| 额外依赖的封装代码 | 生成的 `module.h`。 |

## CGEN-CLI-011 dump 文件名构造

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen src/main.xl --dump-dir dump` |
| 生成规则描述 | 取源文件 base name 去扩展名，和 dumpDir 拼成各产物路径。目录末尾已有分隔符则不重复添加，否则加 `\`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:492`、`demo6/src/driver/xl6_cli.c:506`、`demo6/src/driver/xl6_cli.c:537` |
| 对应生成的 C 代码 | dump 中的 C 文件为 `<dumpDir>/<base>.c`。 |
| 中间形态伪代码 | `dumpPath = dir + sep + base + suffix` |
| 实现同等功能手写的 C 代码 | 手写调试工具也会按源名生成多类产物。 |
| 额外依赖的封装代码 | dump-dir 命令。 |

## CGEN-CLI-012 临时目录与清理规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl` 或 `xl build main.xl -o app.exe` |
| 生成规则描述 | 临时工作目录固定在 `release\tmp\xl6_<purpose>_<time>_<pid>_<random>_<counter>`。build/run 成功或失败都会进入统一 cleanup 出口，默认删除列出的临时文件和工作目录；`--save-temps` 时保留并打印路径。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:553`、`demo6/src/driver/xl6_cli.c:588`、`demo6/src/driver/xl6_cli.c:622` |
| 对应生成的 C 代码 | 临时 C 产物通常是 `module.c`、`module.h`、`module.xapi.json`。 |
| 中间形态伪代码 | `workDir = release/tmp/xl6_build_time_pid_random_id; try work; finally cleanupUnlessSaveTemps` |
| 实现同等功能手写的 C 代码 | 手写 build/run 需要避免临时产物互相覆盖。 |
| 额外依赖的封装代码 | XRT 文件/目录删除 API。 |

## CGEN-CLI-013 run 动态库 PATH 环境

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl --loadlib dep.dll` 或 import 包带 loadlibs |
| 生成规则描述 | run 执行临时 exe 前，会把用户 `--loadlib` 和 importDeps.loadlibs 的目录收集为子进程 `PATH=dir1;dir2;<old PATH>` 环境项。该路径通过 XRT process 的 `arrEnv` 传给子进程，不经过 shell。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:692`、`demo6/src/driver/xl6_cli.c:713` |
| 对应生成的 C 代码 | 不改变生成 C；改变运行时 DLL 搜索路径。 |
| 中间形态伪代码 | `env = ["PATH=" + loadlibDirs + oldPath]; exec(exe, args, env)` |
| 实现同等功能手写的 C 代码 | 手写执行器需要在运行前处理 DLL 搜索路径。 |
| 额外依赖的封装代码 | run PATH env builder、XRT process API。 |

## CGEN-CLI-014 进程参数、路径和链接参数拼接

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--inc-path dir --lib-path dir --link user32` |
| 生成规则描述 | CLI 用固定容量 `XL6_CliProcessArgs` 保存子进程 argv。路径参数不再拼接到 shell 字符串，而是作为独立 argv 项传入 XRT process；需要临时字符串的 `-I`、`-L`、`-l` 和 loadlib def 路径由 args 容器持有并在调用后统一释放。`--link` 值若看起来像路径或文件，则按文件参数直接加入 argv，否则转成 `-l<name>`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:794`、`demo6/src/driver/xl6_cli.c:816`、`demo6/src/driver/xl6_cli.c:827`、`demo6/src/driver/xl6_cli.c:848`、`demo6/src/driver/xl6_cli.c:930` |
| 对应生成的 C 代码 | 不改变 C；生成 TCC argv，如 `-Idir`、`-luser32`、`a.lib`。 |
| 中间形态伪代码 | `args.add("-I"+dir); link(value) = looksLikePath ? args.add(value) : args.add("-l"+value)` |
| 实现同等功能手写的 C 代码 | 手写 TCC 调用必须区分库名和库文件，并管理参数字符串生命周期。 |
| 额外依赖的封装代码 | `XL6_CliProcessArgs`、XRT 内存管理。 |

## CGEN-CLI-015 `--loadlib` import def 生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --loadlib a.dll` |
| 生成规则描述 | 每个 loadlib 先通过 XRT process 调用 TCC `-impdef <dll> -o <def>` 生成 def 文件，再把 def 当链接文件追加到 TCC argv。def 位于当前 workDir 或 `release\tmp`，追加到 argv 时复制路径，避免引用栈上临时缓冲区。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:949`、`demo6/src/driver/xl6_cli.c:980`、`demo6/src/driver/xl6_cli.c:990` |
| 对应生成的 C 代码 | 不改变 C；产生 `xl6_loadlib_N.def` 并链接。 |
| 中间形态伪代码 | `tcc -impdef dll -o def; link(def)` |
| 实现同等功能手写的 C 代码 | 手写 Windows 动态库链接常需要 import library/def。 |
| 额外依赖的封装代码 | TCC `-impdef`、XRT process API。 |

## CGEN-CLI-016 用户 TCC 参数追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | `--inc-path`、`--lib-path`、`--link`、`--loadlib`、`-shared` |
| 生成规则描述 | `--inc-path` 生成 `-I`；`--lib-path` 生成 `-L`；`--link` 按链接规则追加；`--loadlib` 生成 def 并链接；`-shared` 追加 `-shared -DXL6_BUILD_SHARED`。`--emit`、`--emit-header`、`--dump-dir`、`-o` 在 TCC 参数阶段只跳过值。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:999` |
| 对应生成的 C 代码 | `-DXL6_BUILD_SHARED` 可影响生成/包含代码中的 DLL 条件分支。 |
| 中间形态伪代码 | `scan cli options -> append tcc flags` |
| 实现同等功能手写的 C 代码 | 手写 build 系统需要把用户选项转换为编译器参数。 |
| 额外依赖的封装代码 | loadlib def generator。 |

## CGEN-CLI-017 import 依赖 TCC 参数追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg`，pkg manifest 包含 `links` 或 `loadlibs` |
| 生成规则描述 | importDeps.links 逐项按普通链接规则加入。importDeps.loadlibs 逐项生成 def 并链接，和用户 `--loadlib` 共用计数，最多 32 项。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1066` |
| 对应生成的 C 代码 | 不改变生成 C；改变最终 TCC 链接 argv。 |
| 中间形态伪代码 | `for link in deps.links: link(link); for dll in deps.loadlibs: impdef+link` |
| 实现同等功能手写的 C 代码 | 手写包系统需要把包依赖传播到链接器。 |
| 额外依赖的封装代码 | importDeps 收集、loadlib def generator。 |

## CGEN-CLI-018 默认 XRT 与系统库链接

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | TCC argv 总是追加 `-DXL6_USE_XRT_LIFECYCLE`。若用户未显式链接 XRT，则优先使用 `XL6_XRT_OBJ` 环境变量，未设置时回退到 `XL6_XRT_OBJ_PATH`，目标文件必须存在并被链接；若用户未显式链接 TCC runtime，则优先使用 `XL6_TCC_LIBTCC1` 环境变量，未设置时回退到 `XL6_TCC_LIBTCC1_PATH`，且文件存在时追加。最后追加 `Ws2_32`、`IPHLPAPI`、`Shell32`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1108` |
| 对应生成的 C 代码 | `-DXL6_USE_XRT_LIFECYCLE` 控制生成程序使用 XRT 生命周期。 |
| 中间形态伪代码 | `add XRT lifecycle define; link getenv("XL6_XRT_OBJ") || XL6_XRT_OBJ_PATH; link getenv("XL6_TCC_LIBTCC1") || XL6_TCC_LIBTCC1_PATH; link system libs` |
| 实现同等功能手写的 C 代码 | 手写构建需显式链接 XRT 和 Windows 网络/系统库。 |
| 额外依赖的封装代码 | XRT object、TCC runtime、Windows libs。 |

## CGEN-CLI-019 TCC 编译参数总装配

| 项目 | 内容 |
|---|---|
| xlang 写法 | build/run 最终编译 |
| 生成规则描述 | 从默认工具链参数开始，依次追加所有 C 输入、`-o <exe>`、用户 TCC 参数、importDeps TCC 参数，最后追加默认 XRT/TCC runtime/system 链接，并通过 `xl6CliRunProcess()` 用 XRT process 执行 TCC。默认链接放在尾部，确保用户显式 `--link xrt.o` 时仍位于 TCC runtime 之前。无 C 输入或 argv 超过固定容量直接失败。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1134`、`demo6/src/driver/xl6_cli.c:1157`、`demo6/src/driver/xl6_cli.c:1191` |
| 对应生成的 C 代码 | 输入 C 包括生成的 `module.c`、import cSources、用户 C wrapper。 |
| 中间形态伪代码 | `tcc <c inputs> -o exe <user opts> <import opts> <default xrt/runtime/system links>` |
| 实现同等功能手写的 C 代码 | 手写构建需要完整 argv 和链接参数顺序。 |
| 额外依赖的封装代码 | backend、link option、importDeps、XRT process API。 |

## CGEN-CLI-020 头文件输出命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit-header main.h` |
| 生成规则描述 | 头文件输出会打开目标文件，生成 include guard，然后调用 `xl6CgenEmitHeader(root, guard)`。失败时打印 cgen 诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1213`、`demo6/src/driver/xl6_cli.c:1323`、`demo6/src/driver/xl6_cli.c:1471` |
| 对应生成的 C 代码 | 生成 `.h` 声明，不生成 `.c`。 |
| 中间形态伪代码 | `header = cgenHeader(root, guard)` |
| 实现同等功能手写的 C 代码 | 手写 C 混编需要维护导出头。 |
| 额外依赖的封装代码 | cgen header 规则。 |

## CGEN-CLI-021 受检编译单元加载

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任何需要 sem/import 的命令 |
| 生成规则描述 | `xl6CliLoadCheckedUnit` 初始化编译单元，并调用 `xl6CompileUnitLoadFile(path, 1)`。失败时按当前 phase 打印诊断。cgen/build/run/xapi/header/ir/abi 都依赖这一路径。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1236` |
| 对应生成的 C 代码 | 只有加载成功后才进入 cgen。 |
| 中间形态伪代码 | `parse + import expand + sem check` |
| 实现同等功能手写的 C 代码 | 手写编译器同样需要在生成前完成语义检查。 |
| 额外依赖的封装代码 | compile unit pipeline。 |

## CGEN-CLI-022 C 输出文件生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --emit main.c` |
| 生成规则描述 | 打开 C 输出文件后调用 `xl6CgenEmitC(root)`。输出关闭失败或 cgen 返回失败都会导致命令失败；cgen 失败打印诊断。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1246`、`demo6/src/driver/xl6_cli.c:1471` |
| 对应生成的 C 代码 | 完整 `.c` 翻译单元。 |
| 中间形态伪代码 | `emitC(unit.root) -> cPath` |
| 实现同等功能手写的 C 代码 | 手写编译器需要将 AST 后端输出到文件。 |
| 额外依赖的封装代码 | cgen module 规则。 |

## CGEN-CLI-023 XAPI 输出命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl xapi main.xl --emit main.xapi.json` |
| 生成规则描述 | 先构建 `XL6_XapiModule`，再写入 JSON。调用者可传 outModule 复用构建出的 module，用于 build 阶段判断是否有可执行入口。emitPath 为空时写 stdout。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1278`、`demo6/src/driver/xl6_cli.c:1548` |
| 对应生成的 C 代码 | 不生成 C；生成 `xlang-xapi-1` JSON。 |
| 中间形态伪代码 | `module = buildXapi(root); dumpJson(module)` |
| 实现同等功能手写的 C 代码 | 手写扩展库需要导出接口元数据。 |
| 额外依赖的封装代码 | XAPI dump 规则。 |

## CGEN-CLI-024 parse/check/lex/ir 调试命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl lex`、`xl parse`、`xl check`、`xl ir` |
| 生成规则描述 | lex 直接 dump token；parse 可选语义检查后 dump AST text/json 或输出 `ok`；check 加载完整 compile unit 并输出 text/json 诊断；ir 需要受检编译单元并输出 text/json IR。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1354`、`demo6/src/driver/xl6_cli.c:1400`、`demo6/src/driver/xl6_cli.c:1423`、`demo6/src/driver/xl6_cli.c:1439` |
| 对应生成的 C 代码 | 不生成 C；用于对照 lexer/parser/sem/IR。 |
| 中间形态伪代码 | `debug command -> dump selected stage` |
| 实现同等功能手写的 C 代码 | 手写编译器常保留阶段性 dump 命令。 |
| 额外依赖的封装代码 | lexer/parser/sem/ir dump。 |

## CGEN-CLI-025 cgen 命令与 dump-dir 联动

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump --emit main.c --emit-header main.h` |
| 生成规则描述 | 若指定 dumpDir，先执行完整 dump-dir 命令。若没有 emitPath 和 emitHeaderPath，则只做 dump 后返回；否则继续加载受检单元，生成 C 到 emitPath，并可选生成 header。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1471` |
| 对应生成的 C 代码 | `--emit` 产生 C；`--dump-dir` 也会额外产生 `<base>.c`。 |
| 中间形态伪代码 | `dump?; if emit requested: emitC/header` |
| 实现同等功能手写的 C 代码 | 手写工具链可同时输出调试产物和指定目标产物。 |
| 额外依赖的封装代码 | dump-dir command、cgen/header。 |

## CGEN-CLI-026 ABI dump 命令

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl abi main.xl --json` |
| 生成规则描述 | 加载受检编译单元后，根据 jsonOutput 输出 ABI text 或 JSON。emitPath 为空时写 stdout。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1518` |
| 对应生成的 C 代码 | 不生成 C；输出 ABI 调试信息。 |
| 中间形态伪代码 | `dumpAbi(root, json?)` |
| 实现同等功能手写的 C 代码 | 手写 ABI 检查通常需要类似 dump。 |
| 额外依赖的封装代码 | ABI dump 模块。 |

## CGEN-CLI-027 dump-dir 全量产物

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl cgen main.xl --dump-dir dump` |
| 生成规则描述 | dump-dir 会创建 7 个产物：`<base>.tokens.json`、`<base>.ast.json`、`<base>.xlir`、`<base>.abi.json`、`<base>.xapi.json`、`<base>.h`、`<base>.c`。各阶段按顺序生成，任何一步失败即停止。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1562` |
| 对应生成的 C 代码 | dump 目录中的 `<base>.c` 为完整 cgen 输出，`<base>.h` 为导出头。 |
| 中间形态伪代码 | `tokens -> checked unit -> ast -> ir -> abi -> xapi -> header -> c` |
| 实现同等功能手写的 C 代码 | 手写编译器调试包通常也输出全阶段快照。 |
| 额外依赖的封装代码 | lexer、compile unit、IR、ABI、XAPI、cgen。 |

## CGEN-CLI-028 build 必要参数与工作文件

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | build 必须提供 `-o`。收集输入后创建 `build` 工作目录，并固定生成 `module.c`、`module.h`、`module.xapi.json` 三个临时产物。工作目录创建成功后，后续失败统一跳转到 cleanup 出口。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1650`、`demo6/src/driver/xl6_cli.c:1695`、`demo6/src/driver/xl6_cli.c:1699`、`demo6/src/driver/xl6_cli.c:1707` |
| 对应生成的 C 代码 | 主模块临时 C 文件为 `<workDir>\module.c`。 |
| 中间形态伪代码 | `workDir/build -> module.c/h/xapi` |
| 实现同等功能手写的 C 代码 | 手写构建需要中间 C、头文件和接口元数据。 |
| 额外依赖的封装代码 | workDir/temp cleanup。 |

## CGEN-CLI-029 shared build 入口限制

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build lib.xl -shared -o lib.dll` |
| 生成规则描述 | build 先生成 XAPI module。若指定 `-shared` 且 XAPI 判断模块含 xlang main 或顶层可执行语句，则报错：shared build 不能包含可执行入口。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1723`、`demo6/src/driver/xl6_cli.c:1724` |
| 对应生成的 C 代码 | 不输出最终 DLL；避免把 exe 入口打进共享库目标。 |
| 中间形态伪代码 | `if shared && hasExecutableEntry: error` |
| 实现同等功能手写的 C 代码 | 手写 DLL 构建通常禁止 main。 |
| 额外依赖的封装代码 | XAPI entry metadata。 |

## CGEN-CLI-030 build 导出头与 C 输出顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe --emit-header public.h` |
| 生成规则描述 | build 总是先为 wrapper 生成临时 `module.h`，再生成临时 `module.xapi.json`；若用户提供 `--emit-header`，额外生成用户指定头文件；最后生成 `module.c`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1712`、`demo6/src/driver/xl6_cli.c:1719`、`demo6/src/driver/xl6_cli.c:1729`、`demo6/src/driver/xl6_cli.c:1734` |
| 对应生成的 C 代码 | `module.c` 可 include/配合 `module.h` 与用户 C wrapper 编译。 |
| 中间形态伪代码 | `header(temp); xapi(temp); header(user?); cgen(temp)` |
| 实现同等功能手写的 C 代码 | 手写混编应先产出 C 可见声明再编译用户 C。 |
| 额外依赖的封装代码 | cgen header、XAPI dump。 |

## CGEN-CLI-031 build C 输入集合

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl helper.c -o app.exe`，并 import 包带 cSources |
| 生成规则描述 | TCC 输入先加入主 `module.c`，再加入 importDeps.cSources，最后为每个用户 C 源生成 wrapper 并加入。总数受 `cInputs` 固定数组限制，超限报错。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1740`、`demo6/src/driver/xl6_cli.c:1749`、`demo6/src/driver/xl6_cli.c:1758`、`demo6/src/driver/xl6_cli.c:1768` |
| 对应生成的 C 代码 | TCC 编译 `module.c`、包 C 源、`csrc_N.c` wrapper。 |
| 中间形态伪代码 | `inputs = [module.c] + importCSources + wrappers(userC)` |
| 实现同等功能手写的 C 代码 | 手写构建需把包源和用户源一起交给 C 编译器。 |
| 额外依赖的封装代码 | importDeps、C wrapper。 |

## CGEN-CLI-032 build 编译与临时文件清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl build main.xl -o app.exe` |
| 生成规则描述 | build 调用 TCC 编译所有 C 输入到 outputPath。成功或失败都释放 XAPI/compile unit，并清理 module.c、module.h、module.xapi.json、用户 C wrapper、loadlib def；`--save-temps` 时保留。TCC 失败时打印 `tcc build failed`。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1771`、`demo6/src/driver/xl6_cli.c:1779`、`demo6/src/driver/xl6_cli.c:1799` |
| 对应生成的 C 代码 | 临时 C 和 header 默认不保留，最终产物是 `-o` 指定的 exe/dll。 |
| 中间形态伪代码 | `compile(inputs -> output); cleanup temps` |
| 实现同等功能手写的 C 代码 | 手写构建要处理失败诊断和临时产物。 |
| 额外依赖的封装代码 | TCC argv builder。 |

## CGEN-CLI-033 run 编译临时 exe

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl` |
| 生成规则描述 | run 创建 `run` 工作目录，生成 `module.c` 和 `app.exe`。加载受检单元后只生成 C，不生成 header/xapi；TCC 输入为 module.c 加 importDeps.cSources，输出临时 app.exe。工作目录创建成功后，后续失败统一跳转到 cleanup 出口。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1804`、`demo6/src/driver/xl6_cli.c:1836`、`demo6/src/driver/xl6_cli.c:1840`、`demo6/src/driver/xl6_cli.c:1850`、`demo6/src/driver/xl6_cli.c:1864` |
| 对应生成的 C 代码 | 临时 `<workDir>\module.c`。 |
| 中间形态伪代码 | `run: cgen -> tcc -> temp exe` |
| 实现同等功能手写的 C 代码 | 手写脚本运行器也常先 JIT/编译到临时程序。 |
| 额外依赖的封装代码 | TCC argv builder、importDeps。 |

## CGEN-CLI-034 run 程序参数与进程执行

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl run main.xl -- arg1 arg2` |
| 生成规则描述 | run 执行临时 exe 时，将 `--` 后的程序参数逐项加入 `XL6_CliProcessArgs`，空字符串也保留为独立 argv 项；动态库搜索路径通过可选 `PATH=...` env 项注入。执行路径统一走 `xl6CliRunProcess()`，运行返回非 0 则命令失败。成功或失败后按 saveTemps 规则清理 module.c、app.exe 和 loadlib def。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1804`、`demo6/src/driver/xl6_cli.c:1874`、`demo6/src/driver/xl6_cli.c:1887`、`demo6/src/driver/xl6_cli.c:1893`、`demo6/src/driver/xl6_cli.c:1900` |
| 对应生成的 C 代码 | 不改变 C；影响临时 exe 的 argv。 |
| 中间形态伪代码 | `args = argvAfterDoubleDash; env = optionalPathEnv; exec(tempExe, args, env)` |
| 实现同等功能手写的 C 代码 | 手写脚本运行器需要把宿主 CLI 参数传递给目标程序。 |
| 额外依赖的封装代码 | `XL6_CliProcessArgs`、run PATH env builder、XRT process API。 |

## CGEN-CLI-035 CLI 主分派

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl <cmd> ...` |
| 生成规则描述 | `xl6CliMain` 处理 help 和缺失文件参数后，按命令名分派到 lex、parse、check、ir、cgen、run、build、abi、xapi。未知命令返回错误。 |
| 生成器文件名和行号 | `demo6/src/driver/xl6_cli.c:1890` |
| 对应生成的 C 代码 | 不直接生成 C；决定用户请求进入哪条产物路径。 |
| 中间形态伪代码 | `switch argv[1] -> command handler` |
| 实现同等功能手写的 C 代码 | 手写编译器 CLI 也需要集中分派。 |
| 额外依赖的封装代码 | 各命令 handler。 |
