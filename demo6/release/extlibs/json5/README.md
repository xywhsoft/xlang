# JSON5 扩展库

2026-10-25 当前发布版复验：Windows 默认 `test/check.ps1` 全通过，
覆盖严格 C、边界与分配故障、C/XLang 范例同输出，以及核心、边界、
公开范例的独立 EXE AOT 构建运行。WSL `test/check.sh` 通过 C
ASan/UBSan、XLang JIT 边界/分配故障与双语对照。此次没有用新版
Linux 编译器构建独立 EXE，因此 Linux AOT 仍未验收。先前同日
`cannot initialize module ownership policy catalog` 的运行入口故障
不再复现；历史复现见
`../../../dev/extlib_blockers/ownership_catalog_startup_20261025.md`。

`json5.parse(text)` 解析配置文本为 XLang `var`；`valid` 检查语法和本包数据约束，`getValue` 按点号路径返回原始类型，`get` 返回便于展示的字符串。

```xlang
import json5

var config = json5.parse("{host:'localhost', workers:4,}")
println(config["host"])
println(json5.getValue("{workers:4}", "workers"))
```

包内使用 [libpdjson5](https://github.com/boris-kolpackov/libpdjson5) 的 JSON5 事件解析器，上游版本 `ed602821f2929930e5179cd9240d468abf210d60`，授权在 `vendor/UNLICENSE`。数据转换和错误处理只在 `json5_native.c` 实现，内存经 XRT 分配器管理。合法 JSON5 `null` 与失败由 XRT 错误状态区分。重复键拒绝；数字在 int64 范围内保持整数，其余使用 float64。输入上限 64 MiB、嵌套 64 层、容器项数 100 万。

包内 C 边界测试对含注释、数组、嵌套对象和 UTF-8 文本的完整夹具逐字节截断，要求所有严格前缀均被拒绝，之后完整文档仍可解析。该定向回归与固定种子随机字节扫描互补，不代替完整语法模糊测试。

整份源文本必须是有效 UTF-8，注释中的字节也不例外。C 长度入口对非法
UTF-8 报 `XERR_VALUE`；`valid` 返回 `false`，其他 XLang 入口抛 `ValueError`。

十六进制字面量中的 `e/E` 是数字而非指数标记：`0xdead`、`0xBEEF` 和 `-0x1e` 在 int64 范围内均保留为整数；超出 int64 的十六进制数仍按有限 float64 表示。

`valid` 对语法或数据约束不合法返回 `false`；解析器内存不足不是无效文档，
在 C 侧保留 `XERR_MEMORY`，在 XLang 侧抛 `MemoryError`。成功调用清除旧错误。
包内逐点分配故障测试覆盖解析器扩容及 XRT 值构造，确保不交出半成品。
输入字节、嵌套层数或容器项数超限时，C 侧报告 `XERR_RANGE`，XLang
`parse/getValue/get` 抛 `RangeError`；空输入和非法 JSON5 仍为 `ValueError`。
`valid` 对超限返回 `false`，只让内存错误向调用方传播。

`getValue` 的点号路径只适合简单键与数组数字索引；键名含点或解码后含 NUL 时请先 `parse`，再用 `[]` 访问。路径交给 C 前拒绝内嵌 NUL，避免误查询截断后的有效前缀。原始 NUL 源字节不合法，字符串中的 `\u0000` 则按真实长度保留。`get` 只将标量转成文本；数组、对象或含 NUL 的字符串必须用保留类型和长度的 `getValue`。
路径查询先校验整份输入，不能用位于前半段的有效值掩盖后半段的语法错误。

包内对上游词法器做了定点扩展：未加引号的键名按 ECMAScript
`IdentifierName` 识别，例如 `{你好:1}`、`{\\u4F60\\u597D:1}`；
键名里的 `\\uXXXX` 会解码后再比较，重复键照常拒绝。Unicode 空白包含
完整 `Zs` 类别、U+2028/U+2029 和 U+FEFF。
[JSON5 规范](https://spec.json5.org/)使用 ECMAScript 标识符规则；
属性范围冻结为 Unicode 17.0，表文件由 `vendor/gen_unicode_table.mjs` 生成并
随包发布。正常构建只需 C 编译器，不需 Node。包的公开解析入口始终使用内存
buffer；vendored parser 的通用流输入未在本包承诺 Unicode 键名能力。
属性表按 Node.js 提供的 Unicode 17.0 属性生成；Unicode 数据许可见
[Unicode License](https://www.unicode.org/license.txt)。
字符串的 `\u` 代理对会合成为对应 UTF-8 字符；孤立高/低代理项拒绝解析。
超过 int64 的十六进制整数按 float64 表示；包内 C/XLang 回归覆盖
`0x8000000000000000`，成功结果不携带整数探测阶段的残留错误。
显式 `Infinity`、`-Infinity` 与 `NaN` 可解析；有限数字字面量若超出
`float64` 的有限范围则拒绝，不静默变成无穷大。C/XLang 回归同时覆盖
有限的上界值、超大十六进制整数和十进制指数。

这次扩展修复了已知语法缺口，但并不把有限测试写成全规范证明。
2026-10-24 使用更新后的 Windows 发布版 `xl.exe` 复验，默认包门禁完整通过：
严格 C、C/XLang 范例、边界与分配失败用例、核心及公开范例 AOT 均通过。
Linux 包门禁的 C ASan/UBSan、XLang 用例及双语输出对照也通过。
旧发布版的共享符号缺失不再是本包当前阻断；这不等于整个 xlang 发布版已签收。

在 `demo6` 目录执行聚焦验证：

```powershell
./release/xl.exe run release/extlibs/json5/examples/json5_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/json5/test/check.ps1
bash release/extlibs/json5/test/check.sh
```

`examples/json5.xl` 与 `examples/json5_c.c` 成对展示 `parse`、`valid`、
`getValue`、`get` 四个公开入口，逐行输出一致；C 范例统一释放解析值、
路径查询值和返回文本，失败退出也不遗留已持有结果。两份范例各自校验
固定配置的文本、整数、数组与中文键结果；输出相同不是正确性的唯一依据。
C 范例包含包内解析器与原生桥实现，可由 `xl run` 单文件执行；正式包仍由
清单分别编译源文件。

C/XLang 对照范例位于 `examples/`。以下共享缺符号描述属于旧发布版的历史记录。
包 manifest 已声明原生桥实际使用的浮点/整数解析、字符串和值容器模块；
同一 `test/aot_core.xl` 从 3,592,192 缩至 612,864 字节并运行通过。
旧版完整边界 AOT 曾缺共享 `xl6ValueDataSnapshotV1`，此前索引缺符号亦为旧构建快照；上述默认门禁已经完成复验。
Linux C/XL 的 ASan/UBSan 专项门禁为 `bash release/extlibs/json5/test/check.sh`；
它在隔离临时目录编译 C 测试和范例、运行中央与包内 XLang 用例，并逐字节
比较双语范例输出。`test/boundaries.xl` 和 `test/allocations.xl` 已登记到包清单的 `coverage.tests`；在 `demo6` 目录运行
`tools/smoke_extlib_catalog.ps1 -Package json5` 可与中央用例一同复验。
另可指定本地官方 JSON5 2.2.3 源码目录，用 `-ReferenceDir` 运行可选的
474 例固定解析结果对照，其中 286 例还比较解析后的 JSON 值；覆盖键名、数字、转义、注释及嵌套对象/数组组合。该步骤不参与普通构建和日常门禁。显式 Infinity/NaN 无法表示为标准 JSON，只核对解析结果；JS 可表示而 XRT UTF-8 字符串不能表示的孤立代理项，不纳入同结果矩阵，包内 C/XLang 负例仍要求拒绝。这些固定样本不代表完整规范认证。

2026-10-16：补 C/XLang 回归，固定查询命中后仍拒绝尾部垃圾、64 层嵌套
可解析及 65 层拒绝的合同。实现无需修改；Windows 包级门禁与 Linux
ASan/UBSan 通过，完整 AOT 的共享缺符号阻断不变。

同日修复超大有限十六进制字面量被静默解析成 `Infinity`：C/XLang 负例先红后绿，数字入口统一拒绝有限字面量的 `float64` 溢出，显式非有限字面量与最大有限值保持可用。Windows 严格 GCC/C/XLang 对照及核心 AOT、Linux ASan/UBSan 包级门禁通过；包内边界用例纳入目录 smoke，完整 AOT 的共享阻断不变。

同日补 `parse`、`valid`、`getValue`、`get` 各前 24 个分配点的 XLang
错误分类与故障解除后恢复测试。四个入口实际都抛 `MemoryError`，
并修正 `valid` 包装层后备分支及先前写成 `RuntimeError` 的文档。Windows 包级 C/XL、
双语对照与无输出核心 AOT、Linux C/XL ASan/UBSan 整套通过；完整
AOT 共享阻断不变。

2026-10-18：双语教学范例原仅覆盖 `parse`，现补 `valid`、`getValue`、
`get`，C 范例的各失败出口统一清理已拥有结果。Windows `-SkipAot`
完整包门禁与 Linux C ASan/UBSan、XLang、双语输出对照通过；解析器未修改，
完整 AOT 共享阻断不变。

同日用官方 JSON5 2.2.3 源码临时运行可选差分门禁：474 个固定样本的解析成功/失败判定一致，其中 286 个还比对了解析结果；Windows 严格 C、包内 XLang 边界与分配故障、双语范例和核心 AOT 同次通过。参考源码仅在系统临时目录使用并已清理，不进入发布依赖。该固定矩阵仍不等于完整规范认证，完整边界 AOT 的共享阻断未变。

2026-10-19：C/XLang 新增 `\u0000` 转义后的键和值长度回归，并核对
`getValue` 返回完整字符串、`get` 拒绝 C 文本截断。Windows 严格 C、
XLang 边界与裁剪核心 AOT、Linux C ASan/UBSan 和 XLang 联合门禁通过；
解析器实现符合合同，无需修改。完整边界 AOT 的共享阻断未变。

2026-10-19：C 教学范例补齐单文件 `xl run` 入口；Windows/Linux 包门禁
分别将直跑与严格 C 编译结果逐字比较。Windows C/XLang 边界、双语
对照和裁剪核心 AOT，以及 Linux C ASan/UBSan/XLang 联合门禁通过。
完整边界 AOT 的共享缺符号阻断不变。

同日收紧资源错误分类：超过 64 MiB 输入、64 层嵌套或 100 万容器项
统一报告范围错误，空输入和 JSON5 语法错误仍报告值错误；`valid` 对超限
保持返回 `false`。输入大小和深度的 C 红例先失败后通过，XLang
`parse/getValue/get` 的深度异常均已验证。Windows 包级严格 C、XLang
边界与双语范例、Linux C ASan/UBSan 加 XLang 门禁通过；完整带输出
AOT 的共享缺符号阻断不变，不能据此签收整包发布级。

2026-10-20：修复十六进制 `e/E` 被误作指数标记、使 `0xdead` 等 int64 范围内字面量落成 float64 的类型错误。C/XLang 对大小写、负数的类型红例先失败后通过；Windows 包门禁的 C 边界、分配故障、双语输出和核心 AOT，及 Linux C ASan/UBSan、XLang 联合门禁均通过。完整带输出 AOT 的共享主线缺符号阻断未变。

2026-10-21：C 红例发现词法器跳过注释时也跳过了非法 UTF-8 字节。
包的统一解析入口现用 XRT 校验整份源文本，覆盖引号键、字符串值及
单/多行注释；`valid`、`getValue`、`get` 的失败合同也有独立断言。
Windows 严格 C、XLang、双语范例与裁剪核心 AOT，以及 Linux C
ASan/UBSan、XLang 门禁通过。完整带输出 AOT 仍待共享主线符号修复。

2026-10-22：C 边界门禁新增 XRT 活动分配数/字节数检查：
嵌套对象在逐点分配失败与正常完成后均回到解析前基线；
固定种子的 5000 组畸形字节解析并释放后也回到基线。
Windows 包门禁（含 C/XLang、双语范例和裁剪核心 AOT）与 Linux
C ASan/UBSan、XLang 门禁通过。未发现新的包内泄漏，无需修改解析器；
完整带输出 AOT 本轮未复验，不签收整包发布级。
