# DEMO6 RC0 工程基线

本文档记录 DEMO6 进入 RC0 前的工程守门口径。后续开发如果改变这里的边界，应先明确更新本文档和 `rc0_guard.bat`。

## 相关文档

- `RC0工作区清单.md`：记录当前工作区中哪些文件应纳入基线，哪些属于生成物。
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
- 上述 wiring 类静态检查已经直接接入轻量 RC0 守门，包括 docs、cgenrule、tool、batch、source、toolchain、test case、test data、example、C fixture、package fixture 和标准库覆盖/符号检查。
- 批处理字节级检查，允许文件开头 UTF-8 BOM，但正文必须保持 ASCII，避免 Windows `cmd` 在不同代码页下误解析注释或命令。
- 源码 wiring 检查，确保 `src/**/*.c` 要么直接参与构建/专项编译，要么被聚合翻译单元显式 include。
- 工具链路径检查，确保源码中只有 `src/backend/xl6_toolchain.h`、批处理中只有 `tools/xl6_toolchain_env.bat` 可以保存当前 TCC/XRT 默认路径，避免 demo5 工具链路径重新散落到 API/CLI/cgen 实现和测试入口里。
- 手写 C/CRT 安全检查覆盖 `src`、`examples`、`test/cases`、`packages` 中的非生成 C/H 文件，禁止高风险字符串拷贝/拼接/格式化、宽松输入解析、临时文件名、环境变量修改、宽松数值转换、非标准数字转字符串和非移植大小写比较接口回流；完整函数清单以 `tools/check_c_string_safety.ps1` 为准。
- 该手写 C/CRT 安全检查已经直接接入轻量 RC0 守门，不会只依赖 `test.bat` 的完整主回归路径。
- 测试用例 wiring 检查，确保 `test/cases/*.xl` 不会成为未接入的沉睡用例。
- 测试数据 wiring 检查，确保 `test/cases` 下的 JSON/XAPI fixture 可解析、被测试入口或测试源码引用，并且引用不失效。
- 范例 wiring 检查，确保 `examples/**/*.xl` 被主回归、教程、feature suite 或标准库范例入口覆盖。
- C fixture wiring 检查，确保 C/H host 文件和 package native C 文件不会成为未接入的沉睡资产。
- package fixture 一致性检查，确保测试包 manifest、XAPI、native cSources、links/loadlibs 和依赖目标保持同步。
- `lex`、`check`、`parse`、`ir`、`cgen`、`abi`、`xapi`、`run`、`build` 命令面检查。
- 语义错误与 import 错误的文本/JSON 诊断稳定性检查。
- `test_all.bat` 全量测试：依次覆盖 `test.bat` 主回归、`test_tutorial.bat` 教程范例和 `test_dll.bat` 嵌入式 DLL 验收。
- `git diff --check -- demo6` 空白检查。

## 当前基线目录

正式源码与资产：

- `src`：DEMO6 编译器源码。
- `lib/xrt/xrt.h`：DEMO6 当前同步的 XRT 单头文件。
- `test/cases`：语法、语义、代码生成、混编、import/XAPI 等测试用例。
- `examples`：面向使用者的范例程序与 feature suite。
- `packages`：import/package/XAPI 测试与示例资产。
- `docs`：DEMO6 当前唯一设计口径目录。
- `build.bat`、`test.bat`、`test_all.bat`、`test_smoke.bat`、`test_feature_suite.bat`、`test_tutorial.bat`：构建与测试入口。
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
- `src/cgen` 中不得重新出现直接 CRT `malloc`、`calloc`、`realloc`、`free` 调用；生成代码里的内存分配应走 XRT。
- 手写源码中不得新增 CRT 文件路径 I/O：`fopen`、`_wfopen`、`fread`、`fclose`、`fseek`、`ftell`、`rename`、`remove`、`unlink`、`_rmdir`、`rmdir`。文件路径、文件读写、目录和删除能力必须走 XRT。
- `fwrite`、`fflush` 只允许出现在 `src/common/xl6_output.c` 的 stdout 缓冲刷新路径中。
- 命令执行 API 只允许出现在 `src/driver/xl6_cli.c`，用于当前 CLI 调用 TCC、生成 loadlib import def 和运行临时 exe；其他模块不得新增 `system`、`popen`、`WinExec`、`ShellExecute`、`CreateProcess`。
- `src` 中不得重新引入 `xl6*ReadFileText`、`xl6*OpenFile`、`xl6*FileExists`、`xl6*CreateDirAll`、`xl6*StrDupRange`、`xl6_file.c`。统一输出入口 `xl6OutputOpenFile` 例外，标准库声明体 `xl6Std*` 不属于 common wrapper 回退。
- `src` 中不得重新引入 `xl6_time_literal`。
- `driver`、`package`、`common` 中不得使用 CRT `remove`、`_rmdir`、`rmdir` 删除文件或目录。
- `driver`、`package`、`common` 中不得使用 `system("...mkdir...")` 创建目录；目录创建必须走 XRT。
- CLI 临时目录创建必须检查 `xl6CliEnsureTmpDir()` 返回值，不得吞掉目录创建失败。

## XRT 与 CRT 边界

默认原则：

- 文件路径、文件读写、目录创建、内存释放、时间处理优先使用 XRT。
- 能由 XRT 表达的能力，不在 xlang 内部重复制造兼容层。
- 如果 XRT 缺能力，应优先改造 XRT，再回到 xlang 接入。

当前允许的 CRT 使用：

- `driver`、`diag` 中面向命令行的 `stderr` 诊断输出。
- flex/lemon 生成代码内部的 `fprintf` 等实现细节。
- flex/lemon 生成代码内部的 `malloc`、`realloc`、`free` 等实现细节。
- `snprintf` 等用于固定缓冲区格式化命令、路径、C 符号名的安全格式化。
- `fwrite`、`fflush` 仅允许在 `XL6_Output` 的 stdout 缓冲刷新路径中使用。
- `system` 暂时只允许在 `src/driver/xl6_cli.c` 中用于调用 tcc、生成 loadlib import def 和运行临时 exe；不得用于文件、目录、删除等 XRT 已覆盖能力，后续可整体迁移到 XRT process。

当前禁止的 CRT 使用：

- 编译器内部文件读取与写入不得新增 `fopen`、`fread`、`fwrite`、`fclose` 文件路径逻辑。
- cgen/dump 输出不得绕过 `XL6_Output`。
- common 层不得重新增加对 XRT 已有能力的二次包装。

## 诊断基线

编译阶段诊断优先进入 `XL6_DiagList`：

- 词法、语法、语义、import/XAPI、代码生成可恢复错误应尽量进入诊断列表。
- CLI 的 `stderr` 用于流程级错误，例如构建工具缺失、输出文件失败、命令参数错误。
- JSON 诊断输出是稳定接口，不能只保证文本输出可读。
- 新增语义错误或 import/XAPI 错误时，应同步新增负向测试用例。

诊断信息要求：

- 必须包含阶段、文件、行列或相关 AST 节点位置。
- 不使用损坏编码文本。
- 不把内部实现细节暴露为用户唯一可见错误。

## RC0 后续优先级

RC0 前优先处理工程稳定性，不继续扩大语言特性：

1. 守门脚本保持可重复运行。
2. XRT/CRT 边界持续收紧。
3. 错误诊断路径补测试。
4. import/package 再进入正式扩展前，必须先通过 RC0 守门。
