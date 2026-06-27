# demo6/tools 工具目录

本目录保存 DEMO6 的工具链配置脚本和工程守门脚本。脚本用于把“能构建”提升为“边界明确、路径集中、测试资产接线完整、文档不漂移”。

## 工具链入口

- `xl6_toolchain_env.bat`：集中设置 TCC、Flex、Lemon、XRT、默认 include/lib/link 参数。其他批处理脚本需要工具链变量时应先调用它。

## 守门脚本

- `check_batch_wiring.ps1`：检查根目录批处理脚本的基本结构、工具链变量使用、RC0 基线文档登记，以及 smoke/full/tutorial/DLL/RC0 回归入口边界；同时守住单行 TCC C 编译/链接命令必须使用集中 include/lib 变量，守住 `test.bat` 中 `--save-temps` wrapper 用例必须按日志中的确切目录验证并清理，并守住 9 个根目录批处理入口、1 个工具链批处理入口的当前数量基线。
- `check_cli_temp_cleanup.ps1`：动态验证 `xl build` / `xl run` 成功和失败路径不会新增 `xl6_build_*` / `xl6_run_*` 临时目录；捕获 stdout/stderr 必须是严格 UTF-8 且不能混入 PowerShell native error 包装；成功路径 stderr 必须为空，失败路径必须包含预期诊断，并验证 `--save-temps` 保留目录在检查后被安全清理。
- `check_c_fixture_wiring.ps1`：检查 C/H fixture 是否被测试或范例入口引用，package native fixture 是否被同目录 metadata 声明；同时守住入口 C/H fixture 41 个、package C/H fixture 9 个的当前数量基线。
- `check_cgen_statement_expr_allowlist.ps1`：检查 `src/cgen/*.c` 中 GNU statement expression fallback 只能出现在当前白名单模板里；该白名单用于保护仍需左到右求值、短路或临时生命周期的通用表达式 fallback，防止新路径绕过语句级化和 helper 化审查。
- `check_cgen_module_size.ps1`：检查 `src/cgen/*.c` 的模块体积，防止拆分后的代码重新回流到巨型 cgen 文件；当前守住 66 个 cgen `.c` 文件、单文件 2400 行、95000 字节上限，并要求超过 1200 行的 cgen 文件为 0 个。
- `check_c_string_safety.ps1`：扫描手写 C/H 中高风险 C/CRT 接口、常见 CRT 函数手写原型、占位诊断标记和旧式泛化 cgen 兜底诊断，额外扫描手写 lexer/parser 规格文件中的占位标记，并守住 `src` 中绕过 `xl6Console*` / `XL6_Output` 的直接控制台输出、直接 CRT/Win32 命令执行、直接 `exit` / `abort` / `assert` 进程控制，以及 CLI 中 CRT `time()` 回流；同时守住 168 个手写/fixture C/H 文件和 2 个 lexer/parser 规格文件的当前扫描基线。
- `check_cgenrule_docs.ps1`：检查 cgenrule README 全量索引、规则计数、重复 ID、索引引用、占位残留和源码行号引用；同时守住 47 个 cgenrule Markdown 文件和 835 条唯一规则的当前基线。
- `check_risk_ledger.ps1`：检查 `cgenrule` 中 canonical `RISK-*` 风险表，强制当前总账为 21 条唯一风险、18 条已收口、3 条开放，并把开放风险限定为 `RISK-ASYNC-001`、`RISK-CALL-001`、`RISK-LOOP-002`。
- `check_dll_api_boundary.ps1`：检查 `include/xl.h`、`src/api/xl_api.c`、DLL 设计文档和 C 宿主示例中的公开 API 边界一致，并守住 runtime 引用隔离、libtcc 加载失败回滚、libtcc 必需符号校验、集中工具链环境变量、context/依赖路径快照、依赖库句柄所有权和 context 表项提交顺序；同时守住 21 个公开 DLL API 的当前数量基线。
- `check_docs_index.ps1`：检查文档入口、状态矩阵、旧口径残留、未登记 README 和目录级 README 入口；同时拦截旧 demo5 release 路径、Uninit 式卸载命名、RC0 摘要中的旧 `system` 例外口径，以及早期资产数量被写成当前口径；`RC0工作区清单.md` 必须明确标注为历史快照，并守住 32 个 docs Markdown 文件和 12 个额外 Markdown 入口文档的当前基线。
- `check_markdown_links.ps1`：扫描 DEMO6 Markdown 文档中的本地 Markdown 链接，跳过第三方 XRT 单头文件和构建临时目录，拦截断链、非法链接目标和逃出 DEMO6 根目录的本地链接；同时守住 90 个 Markdown 扫描文件的当前数量基线。
- `check_example_wiring.ps1`：检查范例 README、根级范例覆盖模式、tutorial、feature suite 和执行/导出分类接线；同时守住 57 个范例文件的当前数量基线。
- `check_negative_diagnostics.ps1`：静态扫描 `test.bat` 中 xlang 预期失败命令，要求每条命令捕获 stderr、合并诊断、JSON 诊断或 lexer token dump 之一；主回归后以 `-Artifacts` 检查对应诊断产物非空、严格 UTF-8、无替换字符，并防止 PowerShell native error 包装和内部泛化 cgen 诊断混入诊断快照；除循环类型别名和重复 export 这类多位置错误外，正式负例诊断产物不得出现多行；同时守住 250 条预期失败命令、249 个诊断产物、250 条内容断言且 baseline-only 为 0 的当前覆盖基线，防止新增负例只检查退出码。
- `check_package_fixtures.ps1`：检查 package/import/XAPI fixture、身份字段唯一性、自洽性与 `packages/README.md` 索引；同时守住 17 个 package fixture 的当前数量基线。
- `check_release_tmp_artifacts.ps1`：扫描 `release/tmp` 顶层文本产物，要求严格 UTF-8、无替换字符、无 PowerShell native error 包装、无 XRT JSON 解析调试输出、无泛化 cgen 诊断，且生成 C 产物不得包含 GNU statement expression；主回归末尾当前最低文本产物基线为 541 个，RC0 轻量守门允许空产物目录下执行。
- `check_source_wiring.ps1`：检查源码 `.c` 文件是否接入构建或被分片包含；同时守住 91 个源码文件、31 个直接构建源码、60 个分片包含源码和 2 个生成物直接构建入口的当前数量基线。
- `check_source_encoding.ps1`：检查源码、文档、脚本、测试和示例文本是否为严格 UTF-8，并阻止替换字符和典型历史乱码回流；同时守住 1045 个扫描文本文件的当前数量基线。
- `check_stdlib_coverage.ps1`：检查标准库注册表结构、文档、测试和示例覆盖；测试/示例覆盖会忽略注释和字符串；同时守住 180 个公开标准库函数的当前数量基线。
- `check_stdlib_dll_symbols.ps1`：检查 DLL 标准库相关符号、XRT 注入表格式、重复项和边界；同时守住 161 个标准库 DLL C 符号和 346 个 XRT/runtime 注入导出符号的当前数量基线。
- `check_stdlib_examples.ps1`：批量 check/run 标准库范例；脚本会先切到 DEMO6 根目录，避免从仓库根或 CI 调用时相对工具链路径漂移；同时守住 17 个标准库范例文件的当前数量基线。
- `check_stdlib_test_wiring.ps1`：检查标准库测试用例按 `test/cases/...` 完整相对路径接线；同时守住 21 个标准库测试文件的当前数量基线。
- `check_test_case_wiring.ps1`：递归检查 `test/cases` 下 `.xl` 接入主回归，并维护用例前缀索引；同时守住 455 个测试用例文件和 28 个用例前缀的当前数量基线。
- `check_test_data_wiring.ps1`：检查 JSON/XAPI 测试数据 fixture 有效，并按完整相对路径或 XAPI import 语义接线；同时守住 5 个 JSON/XAPI fixture 的当前数量基线。
- `check_tool_wiring.ps1`：检查所有 `check_*.ps1` 是否接入 `test.bat` 和 RC0 守门；同时守住 25 个守门脚本的当前数量基线。
- `check_toolchain_paths.ps1`：防止旧 demo5 路径、旧 XRT release 路径和分散工具链路径回流到源码、批处理和手写 C/H fixture；同时要求 CLI 与 DLL API 都尊重集中工具链环境变量，并守住 126 个源码/头文件、50 个 fixture C/H 文件和 11 个批处理文件的当前扫描基线。

## RC0 入口

- `rc0_guard.ps1`：发布候选轻量守门入口，覆盖必需文件、静态边界、构建、CLI、诊断、文档、风险总账、cgen 模块体积和 wiring 检查；静态输出边界同时禁止 `src/cgen` 回退到 `__xl6_conv_` 旧式 xvalue 标量转换 fallback，并通过白名单锁定剩余 GNU statement expression fallback。

## 维护规则

- 新增 `check_*.ps1` 后必须接入 `test.bat`，并接入 `rc0_guard.ps1` 或在 `check_tool_wiring.ps1` 中明确列为 full-regression-only 例外。
- 新增、删除、重命名守门脚本后必须同步维护本文档。
- 批处理脚本保持 ASCII，中文说明写入 Markdown 或 PowerShell。
- 工具链路径和默认链接参数只在集中入口维护，范例和测试脚本通过变量引用。
