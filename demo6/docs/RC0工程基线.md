# DEMO6 RC0 工程基线

本文档记录 DEMO6 进入 RC0 前的工程守门口径。后续开发如果改变这里的边界，应先明确更新本文档和 `rc0_guard.bat`。

## 相关文档

- `RC0工作区清单.md`：记录早期 RC0 提交前工作区历史快照，用于追溯当时哪些文件应纳入基线、哪些属于生成物；它不代表当前工程规模。
- `RC0变更摘要.md`：记录当前 RC0 基线完成的主要变化、非目标和后续风险。
- `扩展库系统设计.md`：记录 import/package/XAPI/extlib 与多版本共存的长期设计口径。

## 守门入口

RC0 守门命令：

```bat
rc0_guard.bat
```

轻量检查命令：

```bat
rc0_guard.bat -SkipFullTests
```

完整守门包含：

- 必需文件检查。
- 禁止回退静态扫描。
- `build.bat` 构建。
- 文档、工具与批处理 wiring 检查，确保设计文档索引、cgenrule 规则表、所有 `tools/check_*.ps1` 守门脚本和根目录批处理入口不会脱离主回归与 RC0 口径。
- 工具 wiring 检查同时要求每个 `tools/check_*.ps1` 接入 `test.bat`，并且接入轻量 RC0 守门；如果某个检查脚本必须保留在完整主回归中，必须在 `check_tool_wiring.ps1` 中显式登记为完整回归例外。
- 上述 wiring 类静态检查已经直接接入轻量 RC0 守门，包括 docs、cgenrule、Markdown 本地链接、tool、batch、source、toolchain、DLL API boundary、test case、test data、example、C fixture、package fixture 和标准库覆盖/符号检查。
- 工程入口和接口类检查同时守住当前数量基线：79 个源码文件、31 个直接构建源码、48 个分片包含源码、2 个生成物直接构建入口、9 个根目录批处理入口、1 个工具链批处理入口、22 个守门脚本、19 个 DLL 公开 API、3 个 xl.dll C 宿主范例、161 个标准库 DLL C 符号、319 个 XRT/runtime 注入导出符号和 830 个严格 UTF-8 扫描文本文件。
- 文档和静态扫描类检查同时守住当前数量基线：31 个 docs Markdown、12 个额外 Markdown 入口、89 个 Markdown 本地链接扫描目标、47 个 cgenrule Markdown、835 条唯一 `CGEN-*` 规则、114 个工具链路径源码/头文件扫描目标、50 个工具链路径 fixture C/H 扫描目标、11 个工具链路径批处理扫描目标、156 个手写/fixture C/H 安全扫描目标和 2 个 lexer/parser 规格扫描目标。
- 批处理字节级检查，允许文件开头 UTF-8 BOM，但正文必须保持 ASCII，避免 Windows `cmd` 在不同代码页下误解析注释或命令。
- 批处理中的单行 TCC C 编译/链接命令必须使用集中工具链变量：C 编译命令必须带 `%XL6_TCC_INCLUDES%`、`%XL6_TCC_INCLUDES_WINAPI%` 或 `%XL6_TCC_SOURCE_INCLUDE%`；显式链接到 `.exe` / `.dll` 或使用 `-shared` 时，必须带 `%XL6_TCC_LIB_FLAGS%` 或 `%XL6_DEFAULT_LINKS%`。
- 源码编码检查覆盖源码、文档、脚本、测试、示例和 metadata 文本，要求严格 UTF-8，并阻止替换字符和典型历史乱码回流。
- 源码 wiring 检查，确保 `src/**/*.c` 要么直接参与构建/专项编译，要么被聚合翻译单元显式 include。
- 工具链路径检查，确保源码中只有 `src/backend/xl6_toolchain.h`、批处理中只有 `tools/xl6_toolchain_env.bat` 可以保存当前 TCC/XRT 默认路径；示例、测试和 package 的手写 C/H fixture 也会被扫描，避免旧工具链路径重新散落到 API/CLI/cgen 实现、测试入口或宿主示例里。
- 手写 C/CRT 安全检查覆盖 `src`、`examples`、`test/cases`、`packages` 中的非生成 C/H 文件，禁止高风险字符串拷贝/拼接/格式化、宽松输入解析、临时文件名、环境变量修改、宽松数值转换、非标准数字转字符串、非移植大小写比较接口、常见 CRT 函数手写原型声明，以及常见待办、修复、待定、占位和泛化生成错误诊断标记回流；`src` 中还禁止直接 `printf` / `fprintf` / `puts` / `fputs` / `fputc` / `putchar` 控制台输出、直接 CRT/Win32 命令执行、直接 `exit` / `abort` / `assert` 进程控制，CLI 中禁止 CRT `time()` 回流，完整函数清单以 `tools/check_c_string_safety.ps1` 为准。
- 该手写 C/CRT 安全检查已经直接接入轻量 RC0 守门，不会只依赖 `test.bat` 的完整主回归路径。
- 测试用例 wiring 检查，确保 `test/cases/*.xl` 不会成为未接入的沉睡用例。
- 预期失败诊断 wiring 检查，确保 xlang 负例不会只检查退出码；当前数量基线为 245 条预期失败命令、244 个诊断产物、245 条内容断言，baseline-only 必须保持为 0。
- 测试数据 wiring 检查，确保 `test/cases` 下的 JSON/XAPI fixture 可解析、被测试入口或测试源码引用，并且引用不失效。
- 范例 wiring 检查，确保 `examples/**/*.xl` 被主回归、教程、feature suite 或标准库范例入口覆盖。
- C fixture wiring 检查，确保 C/H host 文件和 package native C 文件不会成为未接入的沉睡资产。
- package fixture 一致性检查，确保测试包 manifest、XAPI、native cSources、links/loadlibs 和依赖目标保持同步。
- 上述测试、范例、fixture 与标准库覆盖检查同时守住当前数量基线：446 个测试用例、28 个测试前缀、57 个范例、5 个 JSON/XAPI fixture、17 个 package fixture、180 个标准库公开函数、21 个标准库测试、17 个标准库范例、41 个入口 C/H fixture 和 9 个 package C/H fixture。
- `lex`、`check`、`parse`、`ir`、`cgen`、`abi`、`xapi`、`run`、`build` 命令面检查。
- 语义错误与 import 错误的文本/JSON 诊断稳定性检查。
- `test_all.bat` 全量测试：依次覆盖 `test.bat` 主回归、`test_tutorial.bat` 教程范例和 `test_dll.bat` 嵌入式 DLL 验收。
- `git diff --check -- demo6` 空白检查。

快速冒烟入口：

- `test_smoke.bat` 不再作为 `test.bat` 的别名，而是独立的快速验证入口。
- 当前 smoke 覆盖 `build.bat`、文档/源码/工具接线、`lex`、`parse`、`check`、`cgen`、`abi`、`xapi`、import metadata、容器运行和一个标准库运行样本。
- 完整功能矩阵仍以 `test.bat`、`test_all.bat`、feature suite、tutorial 和 DLL 测试为准。

## 当前基线目录

正式源码与资产：

- `src`：DEMO6 编译器源码。
- `lib/xrt/xrt.h`：DEMO6 当前同步的 XRT 单头文件。
- `test/cases`：语法、语义、代码生成、混编、import/XAPI 等测试用例。
- `examples`：面向使用者的范例程序与 feature suite。
- `packages`：import/package/XAPI 测试与示例资产。
- `docs`：DEMO6 当前唯一设计口径目录。
- `build.bat`、`test.bat`、`test_all.bat`、`test_smoke.bat`、`test_feature_suite.bat`、`test_tutorial.bat`：构建与测试入口，其中 `test_smoke.bat` 是快速冒烟入口，不替代完整主回归。
- `build_dll.bat`、`test_dll.bat`：`xl.dll` 构建、嵌入式运行和宿主 API 边界验收入口。
- `rc0_guard.bat`、`tools/rc0_guard.ps1`：RC0 工程守门入口。

生成物与临时文件：

- `release`：构建输出、运行临时目录、临时可执行文件。
- `src/lexer/xl6_lexer_gen.c`、`src/lexer/xl6_lexer_gen.h`：flex 生成物。
- `src/parser/xl6_parser.c`、`src/parser/xl6_parser.h`、`src/parser/xl6_parser.out`：lemon 生成物。

生成物不得作为设计来源；如果生成物影响构建，应从对应 `.l`、`.y` 或构建脚本修正。

## 已固定的禁止回退项

以下规则由 `rc0_guard.bat` 静态扫描保障：

- `src` 中不得重新出现 `xl6_output_compat`。
- `src/cgen`、`src/common`、`src/abi`、`src/ast`、`src/ir` 中不得使用 `fputc`、`fputs`、`fprintf` 做文本输出。
- `src/cgen` 中不得直接依赖 `FILE*`、`stdout`、`stderr`。
- `src/cgen` 中不得重新出现 `__xl6_conv_` 旧式 xvalue 标量转换 fallback；拥有型 xvalue 转静态标量必须走 release helper，未知 getter 必须显式诊断。
- `src/cgen` 中不得重新出现直接 CRT `malloc`、`calloc`、`realloc`、`free` 调用；生成代码里的内存分配应走 XRT。
- 手写源码中不得新增 CRT 文件路径 I/O：`fopen`、`_wfopen`、`fread`、`fclose`、`fseek`、`ftell`、`rename`、`remove`、`unlink`、`_rmdir`、`rmdir`。文件路径、文件读写、目录和删除能力必须走 XRT。
- `fwrite`、`fflush` 只允许出现在 `src/common/xl6_output.c` 的 stdout 缓冲刷新路径中。
- `src` 中不得新增直接 CRT/Win32 命令执行：`system`、`popen`、`_popen`、`WinExec`、`ShellExecute*`、`CreateProcess*`。CLI 调用 TCC、生成 loadlib import def 和运行临时 exe 必须通过 XRT process API。
- `src` 中不得重新引入 `xl6*ReadFileText`、`xl6*OpenFile`、`xl6*FileExists`、`xl6*CreateDirAll`、`xl6*StrDupRange`、`xl6_file.c`。统一输出入口 `xl6OutputOpenFile` 例外，标准库声明体 `xl6Std*` 不属于 common wrapper 回退。
- `src` 中不得重新引入 `xl6_time_literal`。
- `driver`、`package`、`common` 中不得使用 CRT `remove`、`_rmdir`、`rmdir` 删除文件或目录。
- `driver`、`package`、`common` 中不得使用 shell 创建目录；目录创建必须走 XRT。
- CLI 临时目录创建必须检查 `xl6CliEnsureTmpDir()` 返回值，不得吞掉目录创建失败。
- CLI 临时目录名必须使用 XRT 时间和随机数，不得重新使用 CRT `time()`。

## XRT 与 CRT 边界

默认原则：

- 文件路径、文件读写、目录创建、内存释放、时间处理优先使用 XRT。
- 能由 XRT 表达的能力，不在 xlang 内部重复制造兼容层。
- 如果 XRT 缺能力，应优先改造 XRT，再回到 xlang 接入。

当前允许的 CRT 使用：

- flex/lemon 生成代码内部的 `fprintf` 等实现细节。
- flex/lemon 生成代码内部的 `malloc`、`realloc`、`free` 等实现细节。
- `snprintf` 等用于固定缓冲区格式化命令、路径、C 符号名的安全格式化。

`src` 手写代码中的命令行文本输出必须走 `xl6Console*` 或 `XL6_Output`，确保 Windows 控制台和重定向文件都保持 UTF-8；不得重新直接使用 `printf` / `fprintf` / `puts` / `fputs` / `fputc` / `putchar`。
- `fwrite`、`fflush` 仅允许在 `XL6_Output` 的 stdout 缓冲刷新路径中使用。
- CLI 临时目录名使用 `xrtNow()` 和 `xrtRandomBytes()`，不能重新使用 CRT `time()`。
- CLI 调用 tcc、生成 loadlib import def 和运行临时 exe 已迁移到 `xrtExecCapture` 的 `XPROC_TARGET_EXEC` 参数数组路径，并通过 `xl6Console*` 回写 stdout/stderr；不得重新通过 `system`、shell 字符串或平台私有命令执行 API 绕过 XRT process。

当前禁止的 CRT 使用：

- 编译器内部文件读取与写入不得新增 `fopen`、`fread`、`fwrite`、`fclose` 文件路径逻辑。
- cgen/dump 输出不得绕过 `XL6_Output`。
- common 层不得重新增加对 XRT 已有能力的二次包装。

## 诊断基线

编译阶段诊断优先进入 `XL6_DiagList`：

- 词法、语法、语义、import/XAPI、代码生成可恢复错误应尽量进入诊断列表。
- CLI 的 `stdout` 用于帮助文本和普通成功文本，`stderr` 用于流程级错误，例如构建工具缺失、输出文件失败、命令参数错误；手写 `src` 路径必须通过 `xl6Console*` 或 `XL6_Output` 输出。
- JSON 诊断输出是稳定接口，不能只保证文本输出可读。
- 新增语义错误或 import/XAPI 错误时，应同步新增负向测试用例。
- 新增中文诊断或中文路径相关流程错误时，应确保 `test.bat` 或相应用例覆盖 stderr 重定向后的严格 UTF-8 读取路径。
- RC0 守门脚本捕获命令输出或失败诊断时必须使用 raw stdout/stderr 文件，不允许把 PowerShell native error 包装字段混入守门证据。

诊断信息要求：

- 必须包含阶段、文件、行列或相关 AST 节点位置。
- 不使用损坏编码文本。
- 不把内部实现细节暴露为用户唯一可见错误。
- cgen 兜底诊断必须包含具体 AST kind、类型名或操作符等可定位上下文；不得回退为 `code generation error`、`unsupported statement/expression kind in C generator`、`unsupported type in C generation`、`cannot box unsupported type into var` 或 `unsupported typed index compound assignment operator` 这类无法定位规则分支的泛化文本。

## RC0 后续优先级

RC0 前优先处理工程稳定性，不继续扩大语言特性：

1. 守门脚本保持可重复运行。
2. XRT/CRT 边界持续收紧。
3. 错误诊断路径补测试。
4. import/package 再进入正式扩展前，必须先通过 RC0 守门。
