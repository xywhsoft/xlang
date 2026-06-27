# DEMO6 RC0 工作区清单

本文档是早期 RC0 提交前工作区历史快照。它不替代 `git diff`，只记录快照当时的变更归类，避免正式源码、测试、范例、生成物混在一起。

注意：本文保留创建时的数量和分组，不代表当前工程规模。当前数量基线以 `RC0工程基线.md`、`文档整理收口.md` 和守门脚本输出为准。

## 总览

快照当时 RC0 基线应包含以下正式资产：

- 编译器源码：`src`。
- XRT 单头文件：`lib/xrt/xrt.h`。
- 设计与工程文档：`docs`。
- 测试用例：`test/cases`。
- 测试入口：`test.bat`、`test_all.bat`、`test_smoke.bat`、`test_feature_suite.bat`。
- 范例程序：`examples`。
- package/import 测试资产：`packages`。
- RC0 守门入口：`rc0_guard.bat`、`tools/rc0_guard.ps1`。

快照当时不应纳入正式基线的生成物：

- `release`。
- `src/lexer/xl6_lexer_gen.c`。
- `src/lexer/xl6_lexer_gen.h`。
- `src/parser/xl6_parser.c`。
- `src/parser/xl6_parser.h`。
- `src/parser/xl6_parser.out`。

这些生成物已经由根目录 `.gitignore` 忽略。

## 工作区规模

快照当时非 `release` 文件规模：

- `src`：93 个源码文件，约 1.9 MB。
- `test/cases`：271 个测试输入或 host 文件，其中 `.xl` 251 个、`.c` 20 个。
- `examples`：27 个范例相关文件。
- `docs`：19 个文档文件。
- `lib/xrt/xrt.h`：约 3.0 MB。
- `packages`：9 个 package/import 测试资产。

## 变更分组

建议提交或审查时按以下主题分组。

### 1. 工程守门与基线

相关文件：

- `.gitignore`。
- `demo6/rc0_guard.bat`。
- `demo6/tools/rc0_guard.ps1`。
- `demo6/docs/RC0工程基线.md`。
- `demo6/docs/RC0工作区清单.md`。
- `demo6/docs/RC0变更摘要.md`。

内容：

- 固化 RC0 守门入口。
- 固化禁止回退规则。
- 固化生成物忽略规则。
- 明确正式资产与生成物边界。

### 2. 构建、CLI 与命令面

相关文件：

- `demo6/build.bat`。
- `demo6/main.c`。
- `demo6/src/driver/xl6_cli.c`。
- `demo6/src/backend`。
- `demo6/src/compile`。
- `demo6/src/package`。
- `demo6/test_all.bat`。
- `demo6/test_smoke.bat`。
- `demo6/test_feature_suite.bat`。

内容：

- 接入 build/run/cgen/xapi/import/package 所需命令面。
- 自动链接 XRT/TCC 基础依赖。
- 统一输出文件走 `XL6_Output`。
- 临时文件删除使用 XRT。

### 3. XRT 化与 common 收口

相关文件：

- `demo6/src/common/xl6_common.h`。
- `demo6/src/common/xl6_output.c`。
- `demo6/src/common/xl6_output.h`。
- 删除：`demo6/src/common/xl6_file.c`。
- 删除：`demo6/src/common/xl6_time_literal.h`。
- `demo6/lib/xrt/xrt.h`。

内容：

- 文件读取、文件存在、目录创建、时间解析等能力优先使用 XRT。
- cgen/dump 输出统一进入 `xbuffer` 输出层。
- 删除旧的重复包装与兼容层。

### 4. cgen 拆分与输出去 CRT 化

相关文件：

- `demo6/src/cgen/xl6_cgen.c`。
- `demo6/src/cgen/xl6_cgen.h`。
- `demo6/src/cgen/xl6_cgen_*.c`。
- `demo6/src/cgen/xl6_cgen_*.h`。

内容：

- 将大型 cgen 拆成多个独立分片。
- cgen 内不得使用 `fputc`、`fputs`、`fprintf`。
- cgen 不直接依赖 `FILE*`、`stdout`、`stderr`。
- 输出统一通过 `XL6_Output` 和 XRT `xbuffer`。

### 5. ABI、XAPI、callable 与 import/package

相关文件：

- `demo6/src/abi/xl6_callable.*`。
- `demo6/src/abi/xl6_xapi_dump.*`。
- `demo6/src/abi/xl6_xapi_load.*`。
- `demo6/src/abi/xl6_abi_dump.*`。
- `demo6/src/compile/xl6_imports.*`。
- `demo6/src/package/xl6_package.*`。
- `demo6/packages`。

内容：

- XAPI 元数据生成与读取。
- import 读取 package/interface 信息。
- callable descriptor 与 xcall 基础设施。
- C export ABI、头文件生成和混编测试基础。

### 6. 类型系统、语义检查与诊断

相关文件：

- `demo6/src/type`。
- `demo6/src/sem/xl6_sem.c`。
- `demo6/src/sem/xl6_type_registry.c`。
- `demo6/src/diag/xl6_diag.*`。
- `demo6/src/parser/xl6_parser.y`。
- `demo6/src/parser/xl6_parser_api.c`。
- `demo6/src/lexer/xl6_lexer_api.*`。

内容：

- 类型注册、类型值、泛型、class/custom、func 值等语义检查。
- `check --json` 增加 `phase` 字段。
- 语义错误与 import 错误被 RC0 守门脚本检查。

### 7. 测试与范例

相关文件：

- `demo6/test.bat`。
- `demo6/test/cases`。
- `demo6/examples/feature_suite`。
- `demo6/examples/second_stage.xl`。

内容：

- smoke 测试覆盖基础语法、语义负例和 cgen。
- feature suite 覆盖语言基础、表达式、函数、类型、容器、泛型、闭包、class/custom、C 混编、C export ABI。
- 新增 import/XAPI、typed containers、function value、custom/class 等测试。

## 忽略规则

根目录 `.gitignore` 现在明确忽略：

- 仓库根级 `release`。
- demo5 release 生成物。
- demo6 `release`。
- demo6 根目录误生成的 exe/dll/obj/o/lib/exp/pdb。
- demo6 flex/lemon 生成物。

注意：

- `demo6/lib/xrt/xrt.h` 不忽略，属于当前基线。
- `demo6/packages` 不忽略，属于 import/package 测试资产。
- `demo6/examples` 不忽略，属于范例资产。

## 提交前建议

提交前建议顺序：

1. 运行 `rc0_guard.bat`。
2. 查看 `git status --short -- demo6 .gitignore`。
3. 按本文档的主题分组审查 diff。
4. 确认没有把 `release`、flex/lemon 生成物或临时二进制加入提交。
5. 确认新增测试和范例是有意纳入 RC0 基线。
