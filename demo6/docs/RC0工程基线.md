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
- `lex`、`check`、`parse`、`ir`、`cgen`、`abi`、`xapi`、`run`、`build` 命令面检查。
- 语义错误与 import 错误的文本/JSON 诊断稳定性检查。
- `test_all.bat` 全量测试。
- `git diff --check -- demo6` 空白检查。

## 当前基线目录

正式源码与资产：

- `src`：DEMO6 编译器源码。
- `lib/xrt/xrt.h`：DEMO6 当前同步的 XRT 单头文件。
- `test/cases`：语法、语义、代码生成、混编、import/XAPI 等测试用例。
- `examples`：面向使用者的范例程序与 feature suite。
- `packages`：import/package/XAPI 测试与示例资产。
- `docs`：DEMO6 当前唯一设计口径目录。
- `build.bat`、`test.bat`、`test_all.bat`、`test_smoke.bat`、`test_feature_suite.bat`：构建与测试入口。
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
- `src` 中不得重新引入 `xl6*ReadFileText`、`xl6*OpenFile`、`xl6*FileExists`、`xl6*CreateDirAll`、`xl6*StrDupRange`、`xl6_file.c`。统一输出入口 `xl6OutputOpenFile` 例外。
- `src` 中不得重新引入 `xl6_time_literal`。
- `driver`、`package`、`common` 中不得使用 CRT `remove`、`_rmdir`、`rmdir` 删除文件或目录。

## XRT 与 CRT 边界

默认原则：

- 文件路径、文件读写、目录创建、内存释放、时间处理优先使用 XRT。
- 能由 XRT 表达的能力，不在 xlang 内部重复制造兼容层。
- 如果 XRT 缺能力，应优先改造 XRT，再回到 xlang 接入。

当前允许的 CRT 使用：

- `driver`、`diag` 中面向命令行的 `stderr` 诊断输出。
- flex/lemon 生成代码内部的 `fprintf` 等实现细节。
- `snprintf` 等用于固定缓冲区格式化命令、路径、C 符号名的安全格式化。
- `fwrite` 仅允许在 `XL6_Output` 的 stdout 缓冲刷新路径中使用。
- `system` 暂时允许用于调用 tcc 与辅助工具；后续可迁移到 XRT process。

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
