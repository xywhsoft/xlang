# DEMO6 RC0 工作区清单

本文档记录 RC0 工作区的资产边界。它不替代 `git diff`，只用于把正式源码、测试、范例、扩展库资产和生成物分清楚。

注意：当前数量基线以 `RC0工程基线.md`、`文档整理收口.md` 和守门脚本输出为准。

## 总览

当前 RC0 基线应包含以下正式资产：

- 编译器源码：`src`。
- XRT 单头文件：`lib/xrt/xrt.h`。
- 设计与工程文档：`docs`。
- 测试用例：`test/cases`。
- 测试入口：`test.bat`、`test_all.bat`、`test_smoke.bat`、`test_feature_suite.bat`。
- 范例程序：`release/examples`。
- package/import/extlib 测试资产：`release/extlibs`。
- RC0 守门入口：`rc0_guard.bat`、`tools/rc0_guard.ps1`。

当前不应纳入正式基线的生成物：

- `build/test`。
- `release` 下除 `README.md`、`examples`、`extlibs` 以外的构建输出和临时二进制。
- `src/lexer/xl6_lexer_gen.c`。
- `src/lexer/xl6_lexer_gen.h`。
- `src/parser/xl6_parser.c`。
- `src/parser/xl6_parser.h`。
- `src/parser/xl6_parser.out`。

这些生成物已经由根目录 `.gitignore` 忽略。

## 工作区规模

当前文件规模：

- `src`：122 个源码文件。
- `test/cases`：668 个 `.xl` 测试用例，另有 host 和测试数据 fixture。
- `release/examples`：104 个 `.xl` 范例文件，另有 C/H 宿主、混编和辅助 fixture。
- `docs`：36 个文档 Markdown 文件。
- `lib/xrt/xrt.h`：约 3.0 MB。
- `release/extlibs`：48 个 package/import/extlib 测试资产目录。

## 变更分组

建议提交或审查时按以下主题分组。

### 1. 工程守门与基线

相关文件：

- `.gitignore`。
- `rc0_guard.bat`。
- `tools/rc0_guard.ps1`。
- `docs/RC0工程基线.md`。
- `docs/RC0工作区清单.md`。
- `docs/RC0变更摘要.md`。

内容：

- 固化 RC0 守门入口。
- 固化禁止回退规则。
- 固化生成物忽略规则。
- 明确正式资产与生成物边界。

### 2. 构建、CLI 与命令面

相关文件：

- `build.bat`。
- `main.c`。
- `src/driver/xl6_xlc_shell.c`。
- `src/backend`。
- `src/compile`。
- `src/package`。
- `test_all.bat`。
- `test_smoke.bat`。
- `test_feature_suite.bat`。

内容：

- 接入 build/run/cgen/ABI 元数据/import/package 所需命令面。
- 自动链接 XRT/TCC 基础依赖。
- 统一输出文件走 `XL6_Output`。
- 临时文件删除使用 XRT。

### 3. XRT 化与 common 收口

相关文件：

- `src/common/xl6_common.h`。
- `src/common/xl6_output.c`。
- `src/common/xl6_output.h`。
- 删除：`src/common/xl6_file.c`。
- 删除：`src/common/xl6_time_literal.h`。
- `lib/xrt/xrt.h`。

内容：

- 文件读取、文件存在、目录创建、时间解析等能力优先使用 XRT。
- cgen/dump 输出统一进入 `xbuffer` 输出层。
- 删除旧的重复包装与兼容层。

### 4. cgen 拆分与输出去 CRT 化

相关文件：

- `src/cgen/xl6_cgen.c`。
- `src/cgen/xl6_cgen.h`。
- `src/cgen/xl6_cgen_*.c`。
- `src/cgen/xl6_cgen_*.h`。

内容：

- 将大型 cgen 拆成多个独立分片。
- cgen 内不得使用 `fputc`、`fputs`、`fprintf`。
- cgen 不直接依赖 `FILE*`、`stdout`、`stderr`。
- 输出统一通过 `XL6_Output` 和 XRT `xbuffer`。

### 5. ABI、ABI 元数据、callable 与 import/package

相关文件：

- `src/abi/xl6_callable.*`。
- `src/abi/xl6_abi_dump.*`。
- `src/abi/xl6_abi_json.*`。
- `src/abi/xl6_abi_dump.*`。
- `src/compile/xl6_imports.*`。
- `src/package/xl6_package.*`。
- `release/extlibs`。

内容：

- ABI 元数据生成与读取。
- import 读取 package/interface 信息。
- callable descriptor 与 xcall 基础设施。
- C export ABI、头文件生成和混编测试基础。

### 6. 类型系统、语义检查与诊断

相关文件：

- `src/type`。
- `src/sem/xl6_sem.c`。
- `src/sem/xl6_type_registry.c`。
- `src/diag/xl6_diag.*`。
- `src/parser/xl6_parser.y`。
- `src/parser/xl6_parser_api.c`。
- `src/lexer/xl6_lexer_api.*`。

内容：

- 类型注册、类型值、泛型、class/native-backed class、func 值等语义检查。
- `check --json` 增加 `phase` 字段。
- 语义错误与 import 错误被 RC0 守门脚本检查。

### 7. 测试与范例

相关文件：

- `test.bat`。
- `test/cases`。
- `release/examples/feature_suite`。
- `release/examples/second_stage.xl`。

内容：

- smoke 测试覆盖基础语法、语义负例和 cgen。
- feature suite 覆盖语言基础、表达式、函数、类型、容器、泛型、闭包、class/native-backed class、C 混编、C export ABI。
- 新增 import/package、typed containers、function value、native-backed class/class 等测试。

## 忽略规则

根目录 `.gitignore` 现在明确忽略：

- 仓库根级 `release`。
- demo5 release 生成物。
- demo6 `release` 中除 `README.md`、`include`、`examples`、`extlibs` 之外的构建输出。
- demo6 根目录误生成的 exe/dll/obj/o/lib/exp/pdb。
- demo6 parser trace 输出 `src/parser/xl6_parser.out`。

注意：

- `lib/xrt/xrt.h` 不忽略，属于当前基线。
- `src/lexer/xl6_lexer_gen.c` / `.h` 与 `src/parser/xl6_parser.c` / `.h` 不忽略，属于无 Flex/Lemon 构建的发布输入。
- `release/include` 不忽略，属于随 `xl.exe` 分发的公开 C API 头文件资产。
- `release/extlibs` 不忽略，属于 import/package/extlib 测试资产。
- `release/examples` 不忽略，属于范例资产。

## 提交前建议

提交前建议顺序：

1. 运行 `rc0_guard.bat`。
2. 查看 `git status --short -- demo6 .gitignore`。
3. 按本文档的主题分组审查 diff。
4. 确认没有把 `build/test`、临时二进制或 `src/parser/xl6_parser.out` 加入提交。
5. 确认新增测试和范例是有意纳入 RC0 基线。
