# css

`css` 是受限的 CSS 语法查询器，适合读取简单样式表中的选择器与声明。
它识别注释、引号、括号、声明分隔符以及包含普通规则的 `@media` 等块；
它**不**计算级联、继承、特异性，不展开 `var()`，也不执行选择器匹配。
同一选择器或属性多次出现时，不提供浏览器最终计算样式的承诺。
查询选择器和属性名时，名称两侧的 CSS 注释与空白不参与匹配；
名称中间的注释不把两个 token 拼成一个名称。`selectorAt` 仍返回原始选择器文本。

```xlang
import css

string sheet = "h1 { color: navy; font-size: 24px; }"
assert css.get(sheet, "h1", "color") == "navy"
assert css.countRules(sheet) == 1
assert css.selectorAt(sheet, 0) == "h1"
```

公开 XL 函数：`get`、`countRules`、`countProps`、`selectorAt`、
`validate`、`stripComments`。缺失的文本结果返回空字符串；畸形语法
抛 `ValueError`，负索引抛 `RangeError`。`stripComments` 保留字符串
字面量中的注释样式文本。
`get`、`countRules`、`countProps`、`selectorAt` 在返回查询结果前检查整份
样式表的受支持语法；即使首个匹配位于前半段，尾部损坏也会报错。
这些查询与 `validate` 共用嵌套 at-rule 深度上限：64 层有效，第 65 层
报告 `RangeError`，避免递归扫描耗尽 C 栈。
选择器与声明表达式中的 `()`、`[]` 必须按实际嵌套顺序闭合；
`([)]` 等交叉闭合报告 `ValueError`。两类括号合计最多嵌套 64 层，
第 65 层报告 `RangeError`。查询不会跳过命中规则之后的坏括号。
`stripComments` 只校验注释与引号边界，不替代 `validate`。
引号字符串中的未转义 CR、LF、FF 属于语法错误；反斜杠续行有效，
其中 CRLF 按一个换行处理。这遵循 [CSS Syntax 3 的字符串记号规则](https://www.w3.org/TR/css-syntax-3/#consume-string-token)，
但本包仍只承诺文中列出的受限查询能力。
XLang 的样式表、选择器和属性名若含内嵌 NUL，会在进入零结尾 C 接口前抛 `ValueError`，不会只检查或查询前缀。
原生 C 六个公开入口也要求整份样式表为有效 UTF-8，查询用的选择器和属性名同样须有效；注释中的非法字节不会因被跳过而获准进入，失败报告 `XERR_VALUE`。合法中文文本与名称可正常查询。

C 接口见 `include/css_native.h`。文本结果是**独立拥有型**内存，
调用者必须以 `xrtFree` 释放；`NULL` 表示错误。XL 包装层用
`takeCstr` 直接接管结果，不复制正文。没有全局或线程局部复用结果槽。
计数结果与 `selectorAt` 索引参数使用 `int64_t`，与 XLang `int` 的
C ABI 一致；C 输出计数可使用 `<inttypes.h>` 的 `PRId64`。

包级验证：

- 2026-10-25：当前 Linux 发布版的 `test/check.sh` 通过严格 C
  ASan/UBSan、线程、XLang JIT，以及核心、边界、分配故障和公开范例的
  四条独立 EXE AOT 路径；公开范例 AOT 与 C 输出逐字节一致。
  这是包级验收，不等于完整 CSS 浏览器语义或整个 xlang 发布签收。
- 2026-10-25：六个 C 文本入口不再将 `NULL` CSS 正文视作合法空样式表；
  它们按各自失败值返回并报告 `XERR_ARGUMENT`，真正的 `""` 仍有效。
  C 负例逐入口覆盖错误类别和失败后恢复。Windows 默认门禁的严格
  C/ABI、线程、XLang 与核心/边界/范例 AOT 全通过；Linux C
  ASan/UBSan、线程、XLang 与双语对照通过。当时尚未复验 Linux AOT，
  现由上项门禁补齐。
- 2026-10-25：NUL 校验原先返回同一份 `string`，生成代码为返回值
  复制整份 CSS/选择器/属性输入；现改成无返回值校验，原参数直接交给
  C 查询。公开函数的校验顺序、异常与结果不变。Windows 默认门禁及
  Linux C ASan/UBSan、线程、XLang JIT 与双语门禁已通过；该时点的
  Linux AOT 空白已由上项门禁补齐。
- 2026-10-25：C/XLang 红例确认扫描器原先分别计数 `()` 与 `[]`，
  因而接受 `([)]` 交叉闭合；现改用顺序栈。选择器、声明值、
  后续坏规则及 64/65 层临界值均有回归。当前 Windows 默认门禁的
  严格 C/ABI、线程、分配故障、双语输出与边界/范例 AOT 全通过；
  Linux C ASan/UBSan、线程、XLang JIT 和双语对照通过。
- 2026-10-22：C/XLang 成对范例补齐 `countProps`、`stripComments`，
  六个公开入口均有调用示范；两侧分别断言固定选择器、属性数、
  注释剥离文本和查询值，不再只比较双方打印结果。注释剥离会保留
  token 分隔所需的空格。Windows `test/check.ps1 -SkipAot` 与 Linux
  `test/check.sh`（含 C ASan/UBSan）均通过。解析器/API 未改，完整 AOT
  仍受下述共享缺符号阻断。
- 从 `release` 目录运行 `xl run examples/extlibs/css.xl`，以及
  `xl run examples/extlibs/css_c.c`；两者输出完全一致。
  C 范例显式包含包内实现，可作为单个源文件运行。
- `xl run extlibs/css/test/boundaries.xl`
- `xl run extlibs/css/test/allocations.xl` 对六个公开 XLang 入口分别扫描至首个无故障成功（最多 128 点），每入口至少命中一次 `MemoryError`，每次清除故障后立即重试并核对正常结果。2026-10-21 的 Windows `-SkipAot` 与 Linux 包级门禁均通过；不据此签收完整 AOT。
- 从 `demo6` 目录运行
  `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/css/test/check.ps1`，
  一次验证清单宏严格编译、C ABI、C 边界与线程、XLang 边界和双语同输出；
  去掉 `-SkipAot` 时继续验证无输出核心及完整 AOT。
- Windows GCC `-Wall -Wextra -Werror`、Linux ASan/UBSan 严格测试通过；
  12000 字节长值与四线程 2000 次查询均覆盖。
- Linux C 门禁可复跑：`bash release/extlibs/css/test/check.sh`；复用一份
  XRT 构建运行核心与线程测试。

2026-10-13 增加“有效前缀 + NUL + 损坏尾部”的红绿用例，并复跑 C/XL 边界、线程测试和双语范例。

2026-10-24 使用更新后的 Windows 发布版复验：默认包门禁完整通过，
包括严格 GCC、ABI、C/XLang 边界和分配故障、线程测试、双语范例及
公开范例 AOT。Linux C ASan/UBSan、线程与 XLang 对照门禁也通过。
旧发布版缺少 `xl6ValueDataSnapshotV1` 的阻断已解除；包级门禁通过
不等于完整 CSS 浏览器语义或整个 xlang 发布版签收。
包 manifest 仅声明原生源码使用的 XRT 缓冲与 Unicode 模块；新增无输出
`test/aot_core.xl`，覆盖规则校验、规则数和属性数。从 `demo6/release`
执行 `.\xl.exe build extlibs/css/test/aot_core.xl -o ../build/test/css_core.exe`
后运行产物；首次缓冲模块裁剪从 3,554,816 缩至 576,512 字节。
加入 UTF-8 校验所需的 Unicode 模块后，当前同一核心产物为 577,536 字节，
已构建运行通过。
以下为历史验证记录；共享快照缺符号描述均指当时的旧发布版。

2026-10-15：中央 `test/cases/extlib_css.xl` 的两处畸形样式表负例
改为捕获当前合同规定的 `ValueError`；中央 JIT 与无输出 AOT
均构建运行通过。原生 buffer 模块宏改为条件定义，消除与 manifest
重复声明时的警告。Windows C/XL 边界、Linux ASan/UBSan 与线程
测试复跑通过；带输出范例 AOT 仍缺 `xl6ValueDataSnapshotV1`。

2026-10-15：原扫描器接受字符串内未转义换行；C/XLang 负例先红后绿。
现在统一拒绝 CR/LF/FF，保留反斜杠续行及 CRLF 续行；`get`、
`stripComments` 的同源错误传播也有 C 回归。Windows 严格 GCC、
Linux ASan/UBSan 与线程测试、C/XLang 同输出和无输出核心 AOT 通过；
完整边界 AOT 同日仍缺共享快照符号，不签收发布级。

同日补充 `get`、`selectorAt`、`stripComments` 的逐点分配故障测试：
故障时不交出部分文本，报告 `XERR_MEMORY`，解除故障后可再次调用。
Windows C 聚焦用例与 Linux ASan/UBSan、线程门禁通过；实现未改。

2026-10-16：嵌套 at-rule 原无递归上限，现统一限制为 64 层。C/XL 验证
64 层可查询、65 层的验证与查询均报告范围错误；Windows TCC、严格 GCC
语法、Linux ASan/UBSan 与线程测试、双语对照和裁剪核心 AOT 通过。

2026-10-17：Linux `test/check.sh` 将 C ASan/UBSan 与线程测试、中央和包内
XLang 边界、C/XLang 范例逐字节对照合为一次门禁，已通过。完整 AOT
仍受主线共享快照符号阻断。

同日修复原生扫描器接受非法 UTF-8 的问题：注释中的坏字节原能通过
`validate`，查询和去注释也可能返回非法文本。六个公开入口现在先用
XRT 校验整份 CSS，查询名也校验；C 负例先红后绿，中文选择器、属性和
文本的 C/XLang 正例通过。Linux ASan/UBSan、线程与双语门禁及裁剪 AOT
核心通过；完整带输出 AOT 阻断不变。

2026-10-18：新增 Windows 包内一键门禁；严格 GCC 的清单宏、ABI、C 扫描器、
四线程查询、C/XLang 范例逐字对照均通过。Linux ASan/UBSan 和 XLang
联合门禁复跑通过。默认 Windows 门禁的核心 AOT 运行通过，随后完整边界
AOT 仍缺主线 `xl6ValueDataSnapshotV1`，故暂不签收完整发布形态。

同日追加合法嵌套样本的逐字符替换矩阵，包含结构符号、引号、NUL 与非法
UTF-8 字节；C 测试要求 `validate`、`countRules`、`get` 对整份输入的语法
判断和错误状态一致。Windows 严格 C/XLang 与双语门禁、Linux C ASan/UBSan、
线程及 XLang 对照通过。固定矩阵不是完整 fuzz，完整 AOT 阻断不变。

2026-10-19：C/XLang 红例复现 `a/*comment*/ { color/*comment*/: red; }`
可通过校验但 `get(..., "a", "color")` 返回空值；名称匹配现跳过两侧注释，
但不把 `co/**/lor` 拼成 `color`。仅注释、没有属性名的声明原已拒绝，
新增回归固定这一边界。成对公开范例展示新写法；Windows 严格 GCC、
C/XLang 边界与对照、Linux ASan/UBSan、线程与双语门禁通过。
完整带输出 AOT 仍待共享符号修复，未签收发布级。
