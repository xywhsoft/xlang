# hcl

2026-10-25：四个拥有型文本查询改为导出 `cstring`，由 `xrtFree` 回收，
XLang 调用方仍可直接赋给 `string`。Windows 默认包门禁的严格 C、
线程、语言边界与分配故障、C/XLang 同输出及四份 AOT 均通过；
生成 C 的四条出口使用 `xl6CStringTakeV1`，不走旧的 `takeCstr` 桥接。
Linux C ASan/UBSan 测试通过；当前 Linux `release/xl` 是不认识
`cstring` 的旧产物，且重建前的能力矩阵快照检查未通过，
因此这次修改的 Linux XLang/AOT 仍待新发布物复验。

2026-10-24 当前发布物复验：Windows 默认 `test/check.ps1` 的严格 C、
线程、XLang 边界与分配、C/XLang 同输出，以及核心、边界、分配、
公开范例四项独立 EXE 均通过；Linux `test/check.sh` 的 C ASan/UBSan、
XLang JIT 和 C/XLang 范例对照通过。旧 Windows 共享缺符号阻断
不再代表当前发布物；本轮未构建 Linux 独立 EXE。

`hcl` 是面向配置浏览的**受限只读语法查询器**，不是 Terraform 求值器。
它识别属性、块、标签、注释、字符串及复合表达式边界；表达式保留原文，
不解析变量引用、函数、provider schema，也不生成或修改 HCL。

```xlang
import hcl

string config = "resource \"server\" \"main\" { port = 8080 }"
assert hcl.countBlocks(config, "resource") == 1
assert hcl.blockLabelAt(config, "resource", 0, 1) == "main"
assert hcl.getInt(config, "port") == 8080
```

公开 XL 函数：`get`、`getRaw`、`getInt`、`getFloat`、`getBool`、`has`、
`countBlocks`、`blockLabel`、`blockLabelCount`、`blockLabelAt`、`validate`、
`countAttrs`。`getRaw` 返回表达式原文；`get` 去掉外层双引号并处理
`\n`、`\r`、`\t`、引号、反斜杠及 `\u`/`\U` Unicode 转义，不求值。
属性名、等号、值和块头之间可使用 `/* ... */` 行内注释；注释不会拼接
标识符。解码型查询忽略表达式尾部块注释，但保留字符串内部及表达式
中间的内容；`getRaw` 保留尾部注释原文。
`getFloat` 使用 XRT 严格十进制解析，固定以 `.` 为小数点，不受进程
`LC_NUMERIC` 影响；十六进制浮点和逗号小数均报 `ValueError`。
`getInt` 同样使用 XRT 严格十进制解析，必须消耗完整值；引号内的
前导空白不会被 CRT `strtoll` 默默接受。
缺失的文本属性返回空字符串；畸形语法、错误类型及非法参数
通过 XRT 错误通道在 XL 中抛异常。`validate` 在 XL 中遇到非法语法时抛
`ValueError`，在 C 中返回 `false` 并保留 XRT 结构化错误。
整份 HCL 输入最多 8 MiB UTF-8 字节；超过上限时，所有查询及 `validate`
均在 UTF-8 扫描和解析之前报告 `RangeError` / `XERR_RANGE`，不接受有效前缀。
原生 C 字符串入口只探测上限之外的一个字节，不对超大文本做无界 `strlen`。
`has` 仅对有效名称返回存在与否；空名称在 C 中报告 `XERR_ARGUMENT`，
在 XLang 中抛 `ValueError`，不会伪装成“属性不存在”。
所有查询都会校验整份配置，包括已找到目标之后的内容及已找到块的内部；
块最多嵌套 64 层，超限报 `RangeError`，不会依赖进程栈深度决定结果。
属性表达式中的 `()`、`[]`、`{}` 必须按嵌套顺序闭合，不能交叉闭合；
表达式括号最多嵌套 64 层，超限同样报 `RangeError`。引号中的括号不参与匹配。
XL 公开函数的 HCL 正文、属性名和块类型在进入 C 字符串接口前会拒绝
嵌入 NUL，避免把有效前缀误当成完整输入。
C 解析入口也会在顶层遍历前校验整份 HCL 文本的 UTF-8；引号或注释内的
无效字节不会被当成有效配置，返回失败并设置 XRT `XERR_VALUE`。
双引号内的真实 LF、CR、FF（包括反斜杠后紧跟的真实换行）同样报
`ValueError`；需要换行字符时写 HCL 转义 `\n`，不要把字符串跨物理行。
未知、缺位或越界的 Unicode 转义同样拒绝；`\u0000` 在 `validate()` 和
`getRaw()` 中保留，但解码型查询因 C 字符串结果不能安全表达 NUL 而报错。

C 接口见 `include/hcl_native.h`。文本结果是**独立拥有型**内存，C 调用者
必须以 `xrtFree` 释放；`NULL` 表示错误。XLang 的 `get`、`getRaw`、
`blockLabel`、`blockLabelAt` 返回 `cstring`，直接接管原分配，
可直接赋给普通 `string` 使用；离开生命周期时由运行时释放。
没有全局或线程局部复用结果槽。

包级验证：

- 2026-10-22：C/XLang 成对范例补齐 `getRaw`、`getFloat`、
  `blockLabelCount`、`blockLabelAt`、`countAttrs`，至此覆盖 12 个公开查询入口。
  两侧各自断言注释保留的原文、浮点值、第二个块标签及嵌套属性总数；
  Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh`（含 C ASan/UBSan）
  均通过。只补范例，不改变解析器；完整 AOT 仍受下述共享符号阻断。
- Windows 从 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass
  -File release/extlibs/hcl/test/check.ps1`，同次检查严格 GCC 语法、
  C 边界/线程、中央与包内 XLang、分配故障、双语范例同输出、无输出核心
  AOT、边界与分配故障 AOT，以及带输出范例 AOT。加 `-SkipAot` 只运行 C/XLang 聚焦门禁。
  AOT 产物位于每次独占的 `build/test` 子目录，结束时校验路径并清理。
- `xl run examples/extlibs/hcl.xl` 与 C 范例
  `examples/extlibs/hcl_c.c` 逐行内容一致，均展示 Unicode 转义解码；
  两份范例还分别断言固定文本、数字、布尔、块标签及 Unicode 结果，
  不会仅凭双方打印相同内容就误判为正确；
  Windows 下 C 的行尾为 CRLF、XLang 为 LF。C 范例显式包含包内实现，
  可直接执行 `xl run examples/extlibs/hcl_c.c`；包级运行器会避免重复链接。
- `xl run extlibs/hcl/test/boundaries.xl`
- `xl run extlibs/hcl/test/allocations.xl`：逐点注入分配失败，验证
  `get`、`getRaw`、缺失字段及块标签的 `MemoryError` 分类和恢复。
- `tools/smoke_extlib_catalog.ps1 -Package hcl`：发现包清单登记的边界用例
  及中央用例，不包含线程与 sanitizer 专项。
- 用 `tools/run_extlib_c.ps1 -Package hcl -Source ...` 运行
  `test/test_hcl.c` 与 `test/test_hcl_threads.c`。
- `test/test_hcl.c` 还对上述拥有型文本查询逐个分配点注入失败，
  检查 C API 返回 `NULL`、设置 `XERR_MEMORY`，随后可正常重试。
- Windows GCC `-Wall -Wextra -Werror`、Linux ASan/UBSan 严格测试通过；
  长值 12000 字节和四线程 2000 次查询均覆盖。
- Linux C/XL 门禁可复跑：`bash release/extlibs/hcl/test/check.sh`；包含
  C 的 ASan/UBSan 边界、无效 UTF-8 拒绝、合法中文文本查询、中央与包内
  XLang 用例，以及 C/XLang 范例的逐字节同输出。C 范例使用严格 GCC
  编译，`int64_t` 的格式化在 LP64 与 LLP64 下均受检查。

2026-10-19：C 测试对一份含转义字符串、块与数组表达式的有效文档逐字节
截断，并用六种字节逐位置替换，比较 `validate` 与 `get`、`getRaw`、
`blockLabel`、`countBlocks` 的成功/失败及错误状态；替换为非法 UTF-8
字节必须拒绝。Windows `-SkipAot` 严格 C/线程/XLang、Linux C
ASan/UBSan 与双语包门禁通过。该有限矩阵验证公开入口一致性，
不证明完整 HCL 规范支持；完整 AOT 共享阻断不变。

同日补齐行内块注释：此前属性名与 `=` 之间、块头标签周围的注释会
改变扫描结果，标量后的注释还会使类型化查询失败。C/XLang 正反例固定
注释分隔 token、不拼接标识符、字符串与复合表达式内部保留，以及
`get` 和 `getRaw` 的差别；双语范例展示同一写法。

完整边界 XL AOT 当前因主线缺少 `xl6ValueDataSnapshotV1` 而失败，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`；尚未完成发布验收。
包 manifest 仅带入原生源码实际使用的 XRT 缓冲、字符集和浮点数模块；
新增无输出 `test/aot_core.xl`，覆盖校验、块计数、整数和浮点查询。
从 `demo6/release` 执行 `.\xl.exe build extlibs/hcl/test/aot_core.xl -o ../build/test/hcl_core.exe`
后运行产物；同一用例从 3,565,568 缩至 590,848 字节并通过。
完整边界 AOT 的共享快照缺符号问题仍未解除。

C 公开头的整数结果与索引统一使用 `int64_t`，与 XLang `int` 的 ABI 一致；不能在 LP64 平台用 `long long` 代替。

2026-10-16：C 教学范例原用 `%lld` 直接打印 `int64_t`，在 Linux LP64
的严格 GCC 下失败；改用 `<inttypes.h>` 的 `PRId64`，并把该范例的
Linux 编译运行与双语对照纳入包级门禁。完整 AOT 的共享阻断不变。

2026-10-15：中央 `test/cases/extlib_hcl.xl` 的两处非法语法断言从
`false` 口径修为捕获 `ValueError`，与包内合同一致；中央 JIT 与无输出
AOT 均构建运行通过。原生文件的模块宏改为条件定义，清除与 manifest
同时声明时的重定义警告。Windows C/XL 边界及 Linux ASan/UBSan
复跑通过；带输出范例 AOT 仍缺 `xl6ValueDataSnapshotV1`。

同日补强引号扫描：C/XLang 红绿测试确认真实换行曾被当作合法字符串，
现统一拒绝，并保留 `\n` 转义。Windows 严格 C、XLang 边界与 Linux
ASan/UBSan 复跑通过；完整 AOT 的共享符号阻断不变。
随后统一扫描与取值的转义规则：非法 `\q` 不再静默丢失反斜杠，合法
`\u`/`\U` 解码为 UTF-8，并拒绝解码后嵌入 NUL 导致的结果截断。

2026-10-16：修复查询命中后提前退出、使损坏的后续内容或已命中块体
漏过语法校验的问题。查找回调保留第一次命中，同时遍历整个输入；
统一限定块嵌套为 64 层。C 与 XL 边界、Windows C 包级、Linux
ASan/UBSan 门禁通过；完整 AOT 仍受共享快照缺符号阻断。

2026-10-15：`getFloat` 从区域设置敏感的 CRT `strtod` 改为 XRT 十进制
解析；Windows 德语小数区域设置的 C 回归先红后绿，并覆盖逗号小数拒绝。
Windows 严格 GCC、C/XLang 边界、Linux ASan/UBSan 与裁剪 AOT 核心
通过；探针实际调用 `getFloat` 后为 596,480 字节。完整带输出 AOT 的
共享缺符号阻断不变。

2026-10-16：C 红色用例确认 `([)]`、`({)}` 原被三个独立计数器误认为
合法表达式；现以 64 层有界栈校验闭合顺序。C/XL 验证交叉闭合拒绝、
合法嵌套、引号内括号和 64/65 层临界值；Windows C/XL、四线程、
Linux ASan/UBSan 与双语范例通过。完整 AOT 阻断未变。

2026-10-17：C 与 XLang 增加逐分配点故障回归，覆盖解码查询、原文查询、
缺失字段及块标签。Windows C/XL 与 Linux ASan/UBSan 包级门禁通过；
完整 AOT 的共享快照缺符号阻断未变。

2026-10-18：`has` 原将空属性名直接当作缺失，且可能保留先前错误；C/XLang 负例先红后绿。现在空名称与其他属性查询统一报参数错，有效查询可清除错误。Windows C/XLang 定向用例和 Linux C ASan/UBSan、XLang、双语门禁通过；完整 AOT 共享阻断未变。

2026-10-17：C 负例复现 `getInt(" 42")` 被 CRT 接受，与本包严格浮点
转换不一致；现使用 XRT 长度感知整数解析，并声明整数模块。
C/XLang 负例与合法负整数恢复通过；Windows 严格 GCC/JIT、Linux
ASan/UBSan/XLang/双语范例通过。当前无输出核心 AOT 构建运行通过，
完整边界 AOT 实测仍缺 `xl6ValueDataSnapshotV1`，未发布签收。

2026-10-18：新增 Windows 单入口包级门禁，复用已有 C 边界/线程与
XLang 中央/包内/分配故障用例；严格 GCC 语法、双语范例逐行同输出和
无输出核心 AOT 同次通过。未改库实现；完整带输出 AOT 仍受共享
`xl6ValueDataSnapshotV1` 阻断，不据此签收发布级。

2026-10-20：Windows 包脚本原未声明 `-SkipAot`，即使传入该参数仍照常
构建核心 AOT，也从不验证带输出范例。现默认运行两级 AOT，并将范例输出
与 C 对照；`-SkipAot` 路径在 PowerShell 5.1/7 通过。当前默认门禁的核心
AOT 通过，带输出范例因共享 `xl6ValueDataSnapshotV1` 缺符号正确失败；
失败后无本次构建目录残留，整包仍未发布签收。
