# DEMO6 测试资产入口

本目录保存 demo6 的回归测试资产。`test/cases` 下的 `.xl` 文件会被递归纳入用例 wiring 守门，由 `test.bat`、范例批处理和工具脚本接线；`.c`/`.h` 宿主 fixture 由 C fixture 守门检查；`.json`/`.xapi.json` 数据 fixture 由 test data 守门检查。

## 主要入口

- `test.bat`：主回归入口，覆盖构建、静态守门、功能用例、标准库、import/package、CLI/DLL 等路径。
- `test_smoke.bat`：快速冒烟入口，覆盖构建、核心文档/源码/工具接线、CLI 基础阶段、ABI/XAPI、import、容器运行和一个标准库运行样本。
- `test_all.bat`：完整回归入口，串联主回归、tutorial 和 DLL 测试。
- `test_tutorial.bat`：教学范例回归。
- `test_feature_suite.bat`：功能范例套件回归。
- `test_dll.bat`：`xl.dll` 嵌入式运行库回归。

## fixtures

- `fixtures/libprobe`：package/import/XAPI 测试用裸包名 fixture，用于验证 `XLANG_PACKAGE_PATH=release\extlibs;test\fixtures` 下的非发布物搜索路径。

## 用例前缀索引

以下前缀来自 `test/cases` 下递归发现的 `.xl` 用例。新增新前缀时，必须同步更新本节，避免测试资产变成只有批处理知道。

<!-- case-prefix-index:start -->
- `anonymous`：匿名函数基础用例。
- `c`：C ABI 类型用例。
- `call`：调用参数、默认参数和命名参数用例。
- `cgen`：代码生成、运行时 lowering 和生成 C 快照用例。
- `class`：class/custom 基础语义用例。
- `cli`：CLI link/loadlib 用例。
- `comments`：注释词法语法用例。
- `containers`：容器基础语义用例。
- `control`：控制流用例。
- `export`：xlang 导出 C ABI 用例。
- `extern`：C extern 函数/变量用例。
- `function`：function 类型基础用例。
- `generic`：泛型类型基础用例。
- `generics`：泛型语法基础用例。
- `import`：import/package/XAPI 导入用例。
- `invalid`：无效运算和无效 range 负例。
- `layout`：布局和语句分隔用例。
- `member`：成员与索引用例。
- `operators`：运算符基础用例。
- `overload`：可变参数重载用例。
- `overloads`：重载基础用例。
- `parse`：解析器负例和边界诊断用例。
- `sem`：语义诊断负例。
- `stdlib`：标准库函数与错误路径用例。
- `type`：type 值和类型别名用例。
- `varargs`：可变参数基础用例。
- `xapi`：XAPI doc、JSON 转义和类型字段用例。
- `xl`：`xl run` 参数用例。
<!-- case-prefix-index:end -->

## 守门脚本

- `tools/check_test_case_wiring.ps1`：所有 `test/cases` 下递归发现的 `.xl` 必须按相对路径接入回归入口，且本 README 的前缀索引必须与当前用例一致。
- `tools/check_test_data_wiring.ps1`：所有 JSON/XAPI 数据 fixture 必须有效，并通过完整相对路径或 XAPI import 语义被引用。
- `tools/check_c_fixture_wiring.ps1`：所有 C/H fixture 必须被测试、范例、DLL 或同 package 目录内 metadata 接线。
- `tools/check_stdlib_test_wiring.ps1`：所有 `test/cases/stdlib_*.xl` 必须按完整相对路径接入主回归。
