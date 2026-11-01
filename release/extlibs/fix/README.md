# fix

## validate不物化字段（2026-11-02）

`parse` 与 `validate` 现在共用一条私有扫描路径，仅前者创建字段对象树；
后者仍检查完整字段顺序、标签/数值语法、单帧/字段上限、BodyLength
和CheckSum，不变成只检查头尾的快速猜测。不改API/ABI、所有权、
三项XRT模块依赖或格式范围。旧实现正常语义未发现错误，本轮是
资源成本优化，不把减少分配误称为新增功能或行为BUG修复。

合法报文的原生C谓词调用不再复制字段或创建随后即释放的值树；
仍须扫描NUL结尾长度、字段与校验和，并清除旧线程诊断。
非法报文仍可能在构造诊断时分配，诊断OOM继续保留 `XERR_MEMORY`；
不能把它当普通无效报文静默转为false。XLang侧还含NUL校验、参数
适配与异常处理成本，不据此宣称整条语言调用链零开销。

新增 `test/validate_cost_c.c`：独立组帧，检查0..4096应用字段、空/
等号/中文/非UTF8字节原文、全部截断、坏校验和、4097字段、NULL、
解析结果/顺序/输入不变、七内存指标与诊断预算。原生合法谓词
还在0分配预算下执行，输出28份parse/validate实际分配记录；
解析仍需其原有结果分配，不强制不同引擎的绝对分配数相同。
原374条Python独立语法/字节参照、旧边界与真实生成原型全部保留。
四条故障扫描现在明确区分合法validate的不分配路径和其余故障路径。

新增 `test/validate_budget.xl` 保留整个语言调用的真实预算验证；
`examples/validation.xl` / `validation_c.c` 仅对帧校验通过的输入解析，
共同匹配五行黄金输出，默认无网络/文件。它不是交易数据字典/会话
验证，也不替代原始语言数组传入 `build` 的M1正例。

近期完整默认门禁与成本结果见根目录
`docs/reports/extlib-fix-validate-20261102.json`；后面的旧日期记录
不代表当前整包发布签收。

本轮Windows57阶段前56通过、55输入/工具指纹不变；Linux原生61阶段
前60通过、52来源/四工具/boot保持，C含ASan/UBSan/泄漏。原八实际
原型、374独立记录、C上限与故障、双语黄金输出及各九份普通AOT过。
最后原中央数组构帧仍M1，最小源两平台check/build0、JIT/AOT1，默认
门禁仍退出1；后续原语言路径未执行，不记为通过或改为预期失败。

同一最终C成本夹具两平台90场景：旧56条“合法谓词不物化/零预算”
质量断言未满足，新0；这不是旧语义错误。28合法形状中parse分配
逐条不变，validate在0/1/64/512/4096应用字段分别从29/36/520/
4112/32792次XRT分配变为0。另496次诊断预算中新62实际OOM，
均分类正确、可恢复，七内存指标保持；旧核心的374独立语义记录也
逐字相同，不只对比新实现自校验。

同一GCC-O2、带MEMORY_DEBUG宿主，三轮交替旧新顺序：Linux固定
5000次单字段/1000次64字段/200次512字段的中位CPU毫秒分别
30.734→0.439、109.433→1.350、158.957→1.734。Windows新路径
多为0–1ms，已接近clock分辨率，只报原始值、不计算速度比。
此成本模型包含内存诊断开销，不外推生产吞吐；未测parse吞吐。
核心对象text在Win/Linux为-128/-216B，data/bss不变，也不是最终
完整程序体积。C字节保留测试不改变XLang源文件UTF8规则。

## 前次状态（2026-10-30）

本轮修复 `parse` / `validate` 把 `08=`、`09=`、`035=`、`010=` 等带前导零
标签接受为有效帧的问题。生产修改只在共享 TagNum 读取处增加检查，不改
公开 API/ABI、所有权、BodyLength 数值语法或字段值。新测试374组覆盖四种
标签位置、0..32个前导零、非法数字/边界及两类值文本，旧实现256项失败，
修复后0；这是组合失败数，不是256个独立 BUG。另四条逐点 OOM/立即恢复
路径核对活动分配/字节与非法/重复释放。

Windows 预验中的原 C、独立三模块构建、八条实际 cgen 原型、三引擎
374条 Python 独立字节参照、新双语黄金例与五个独立程序通过。
原中央语言数组传入 `build` 的正例仍在共享 M1 边界失败，默认门禁仍
非零退出，不将该正例改为预期失败，也不以新解析范例代替构帧闭环。
最终双平台报告见根目录 `docs/普通扩展库发布验收矩阵_2026-10-28.md`
及任务台账；外部网站未部署，旧日期记录不代表当前源码已获发布认证。

## 历史阻断说明

2026-10-25 当前 Windows 发布物复验：C 原生测试通过，XLang 的单字段
最小用例在 `check` 通过后，于 JIT 和独立 EXE 的 `build(var fields)`
调用中报数组类型错误；完整语言边界用例在第一条同形调用停止。
复现与主线修复方向见
`../../../dev/extlib_blockers/fix_var_array_projection.md`。以下较早的
语言/AOT 通过记录不代表当前发布物已通过完整包级门禁。

## 用法与合同

`fix` 是 FIX 4.x 文本报文的**帧编解码器**，不是交易会话引擎。它自动生成
`8=BeginString`、`9=BodyLength`、`10=CheckSum`，按传入顺序保留应用字段，
也保留重复标签。它不连接交易所、不管理序号/重发，也不验证场所数据字典。

```xlang
import fix

var fields = [fix.field(35, "D"), fix.field(49, "SENDER"), fix.field(56, "TARGET")]
string message = fix.build("FIX.4.4", fields)
assert fix.validate(message)
var decoded = fix.parse(message)
```

`field(tag, value)` 创建有 `tag` 与 `value` 的对象；`build(beginString, fields)`
接受这些有序对象，也接受同结构的对象，其值可为字符串、有符号或无符号整数、浮点或布尔值。
浮点字段使用 XRT 写出区域无关的十进制文本，小数点固定为 `.`，
不受进程 `LC_NUMERIC` 影响。
应用字段不可使用保留标签 8、9、10，文本值不可含 SOH (`\x01`) 或 NUL。
标签须为正整数的十进制写法，不允许前导零、正负号或空白；例如 `35=`
有效，`035=` 无效，即便帧长度和校验和正确也拒绝。这来自
[FIX 官方 TagNum 说明](https://forum.fixtrading.org/t/fix-standard-errors-and-omissions-2018-2021/14231/13)，
不是对所有数字文本的统一限制：`9=0008` 等 BodyLength 和 `35=0007`
等应用字段值仍保留各自语义，三位 CheckSum 仍按需要补零。标签范围沿用
本包 C `int64` 正数范围；不以帧校验冒充交易场所数据字典验证。
XLang 的 `field`、`build`、`parse` 在交给 C 字符串接口前拒绝内嵌 NUL；
`validate` 对这类输入返回 `false`，不会把后缀静默丢弃。C 公开操作
会清理前一次操作遗留的 XRT 错误状态。
单帧限制 16 MiB、最多 4096 个**应用字段**；8、9、10 是额外的帧字段，
所以最大合法报文的解析结果含 4099 项。`parse` 返回 `{valid, fields}`，
其中 `fields` 包含 8、9、10；格式无效时抛 `ValueError`，超过 16 MiB
或字段数上限时抛 `RangeError`。
`validate` 对无效报文返回 `false`，不留下预期解析错误；内存分配失败
不是无效报文，C 侧保留 `XERR_MEMORY`，XLang 侧抛 `MemoryError`。
`field`、`build`、`parse` 的原生分配失败在 XLang 侧同样抛 `MemoryError`，
格式或参数错误仍抛 `ValueError`，容量超限抛 `RangeError`。C 可通过
`xfix_has_error`、`xfix_memory_error`、`xfix_range_error` 在失败后检查
本线程的 XRT 错误状态。
构建和解析都要求 `8=BeginString` 非空；校验和即使正确，空 `8=` 也不是有效帧。
原生 C 的 `build` BeginString 与 `field` 文本入口在读取 NUL 终止符时也按
16 MiB 限界扫描；超过上限返回 `XERR_RANGE`，不会先无界扫描或分配字段对象。
单独的 `field` 文本可恰好为 16 MiB；完整报文仍须为 8、9、10 字段留出空间，
因此该极限字段不能构建成符合单帧上限的报文。

对应的 C 声明在 `fix_native.h`。`xfix_build` 返回独立的 XRT 缓冲区，
C 调用方用 `xrtFree` 释放；`xfix_field`/`xfix_parse` 返回的 `xvalue*`
用 `xrtValueRelease` 释放。XLang 包装会处理这些所有权。
`examples/fix_c.c` 可在仓库的 `release` 下直接运行：

```powershell
./xl run extlibs/fix/examples/fix_c.c
```

范例为单文件运行显式包含 `fix_native.c`；正式扩展库仍通过包清单将
`fix_native.c` 作为独立 C 源文件编译，不应在同一构建中重复加入它。

在仓库根目录运行默认门禁（不带原 `` 前缀）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/fix/test/check.ps1
bash release/extlibs/fix/test/check.sh
```

需要 GCC 与 Python3.10+，可通过 `CC` / `PYTHON` 指定路径；缺少工具
必须失败，不静默跳过。Windows `-SkipAot` 只供诊断，不是发布验收。
门禁先运行原 C、三模块独立构建、新标签矩阵、内存故障、实际原型、
五个独立程序，再强制原中央/包内语言数组构帧、分配故障与双语范例。
当前会在原中央正例的 M1 处停止，后续未执行项不记通过。
Linux 增加 ASan/UBSan/泄漏检查，独占构建目录与逐阶段日志失败时保留；
两平台前后核对输入指纹，成功只清理本次目录。生产依赖仍为三个模块，
测试运行库只另加 MEMORY_DEBUG，不用 ALL 实现掩盖依赖。

`test/tag_grammar_c.c` 独立组帧（不调用产品 build），Python
`test/check_tag_records.py` 再独立生成同一语料的原始字节、长度、校验和
及预期结果；GCC、TCC 内存执行和独立程序均比较完整374条记录，不只
核对最终 PASS。默认新矩阵另包含 parse/validate × 正常/畸形标签
四条分配预算和立即恢复路径；合法validate不再要求人为触发OOM。
原4096/4097字段及16 MiB边界测试保留。
包内 `test/tag_grammar.xl` 覆盖四个标签位置，并确认 BodyLength 的前导零
和应用字段原文不受影响，已登记 coverage.tests。

`examples/tag_gate.xl` / `tag_gate_c.c` 展示相同外部报文字节的接收校验、
读取原文值、畸形标签拒绝、格式错误与恢复，五行输出共同匹配
`examples/tag_gate.expected.txt`。可从发布目录分别 `xl run`，不写文件。
这不是给数组构帧 M1 增加旁路工厂，旧构帧正例仍然必须通过。

以下是原双语构帧门禁与对应日期的历史结果，不能覆盖当前 M1：

C 与 XLang 的下单报文范例
在同一脚本中逐字对照，并由独立 Python 检查器按原始字节重算示例
报文的 `9=BodyLength`、`10=CheckSum`，核对字段顺序；此检查不调用
本包的 `validate`，Windows/Linux 包级门禁均需 Python 3。
当时 Windows 严格 GCC、C/XLang 边界、双语范例及 Linux 严格 GCC +
ASan/UBSan C 边界通过；4096/4097 应用字段及最大字段数报文回读也已覆盖。
四个公开入口的首次分配失败、错误分类与恢复由 C 和 XLang 专项测试覆盖。
当时完整 AOT 在 `xl6ValueDataSnapshotV1` 缺符号处
阻断，见 `dev/extlib_blockers/text_snapshot_link.md`，尚未签收完整发布门禁。
新增 `test/aot_core.xl` 独立检查构建帧与校验的 AOT 主路径；包 manifest
只声明原生源码使用的 XRT 缓冲、值容器与浮点数字模块。同一核心产物从
3,595,776 缩至 641,024 字节，执行通过；完整边界 AOT 的共享阻断不变。

2026-10-15：修复逗号小数点区域设置下浮点字段被写成 `44=1,5` 的问题；
Windows C 真实报文负例先红后绿，C/XLang 包级门禁及无输出核心 AOT 通过。

2026-10-17：Linux `test/check.sh` 在 C ASan/UBSan 测试后运行中央主用例、
包内边界和分配失败用例，并逐字节对照 C/XLang 范例输出；两份包内
XLang 用例均登记到 `coverage.tests`。整套门禁通过。
完整 AOT 的共享链接阻断不变。

2026-10-17：原生 C `xfix_parse` 原在检查 16 MiB 单帧上限前无界
`strlen`。16 MiB 加 1 字节的未终止输入由 ASan 复现越界读取，现最多
探测上限再加一个字节；当时超限返回 null 值和 `XERR_ARGUMENT`，
现已统一改为 `XERR_RANGE`。
Linux C ASan/UBSan、XLang/双语联合门禁及 Windows `-SkipAot` 包级门禁
通过。C 接口仍要求有效 NUL 结尾字符串，完整 AOT 阻断未变。

2026-10-18：补齐另外两处 C 文本入口的相同边界。`build` 的 BeginString
原先无界 `strlen`，超限误报 `VALUE`；`field` 先无界 `strchr/strlen`，
还能构造超过单帧上限的字段。两条 C 负例先红后绿，且对无终止符
缓冲区按限额返回 `RANGE`。Windows 严格 GCC/C/XLang/双语及 Linux
ASan/UBSan 与 XLang 对照门禁通过；完整 AOT 的主线缺符号阻断不变。

2026-10-19：C 字符串扫描原在恰好 16 MiB 的终止符处提前报超限；
新增红绿回归后接受该长度的独立 `field` 文本，继续拒绝再长一字节和
上限附近无终止符输入。Windows 严格 C/XLang 包级门禁与 Linux C
ASan/UBSan、XLang、双语范例门禁通过；完整 AOT 的共享阻断不变。
当前发布版直接构建完整 XLang 范例时，首个缺失符号为
`xl6ValueDataSnapshotV1`；旧 `xl6ValueIndexReadCopyV1` 记录是历史快照。

2026-10-19：修复 C 范例单独 `xl run` 缺 `xfix_parse` 等实现符号的问题。
Windows 严格 GCC、单文件 TCC、C/XLang 同输出及 Linux C ASan/UBSan、
Linux TCC/XLang 同输出门禁通过；正式包构建仍分离编译原生实现，
完整 AOT 的共享缺符号阻断不变。

2026-10-20：`build` 原先拒绝 XRT `XVALUE_UINT`，与公开的“整数字段值”
口径不一致。C 以 `UINT64_MAX` 红测复现后，原生字段写出器增加无符号
十进制分支；XLang `uint64` 字段值同步回归。Windows 严格 C/XLang 与
双语范例、Linux C ASan/UBSan 和 XLang 联合门禁通过。完整 AOT 本轮
未重验，仍按已登记的共享缺符号阻断处理。

同日包级 Windows 门禁改为每次使用独占构建目录并重新编译 XRT，结束时
仅清理本次目录，不复用固定路径下的对象文件。`-SkipAot` 的 C/XLang
边界与双语范例通过；默认门禁的核心 AOT 也通过，但边界用例构建缺
共享 `xl6ValueIndexReadCopyV1`，失败后目录已清理。前述完整范例缺
`xl6ValueDataSnapshotV1` 是不同构建目标的记录，二者均未关闭。

`build` 与 `parse` 的 C 回归现逐点注入前 128 个可能分配位置，要求确实
触发多个失败点；每次内存失败不得交出报文或半成品对象，且故障解除后
原报文仍可解析。当前 Windows 严格 C/XLang 包门禁与 Linux C
ASan/UBSan、XLang 双语门禁通过；未发现需修改的包内编解码逻辑。

2026-10-22：上述逐点故障扫描进一步核对每次结果释放后的 XRT 活动
分配数与字节数是否回到调用前基线，覆盖成功和内存失败路径。Windows
`test/check.ps1 -SkipAot` 与 Linux `test/check.sh` 的 C/XLang 双语门禁
通过；完整 AOT 仍受共享缺符号阻断，本轮不签收整包发布级。

2026-10-23：XLang 分配故障用例原只在每个入口的第一个分配点使用单名
`catch MemoryError`，实际上是无类型捕获变量。现在 `field`、`build`、
`parse`、`validate` 各扫描前 64 个注入点，并用 `catch MemoryError error`
核对故障分类、未触发时成功及故障解除后的调用恢复。Windows `-SkipAot`
包级门禁和 Linux C ASan/UBSan、XLang/双语联合门禁通过；实现与公开 API
未变，完整 AOT 的共享阻断未解除。
