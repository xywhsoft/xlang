# color

2026-10-27：八个独立 XRT 文本结果改由 `@owned string` 直接接管，
C 原型明确返回 `char*`；语言包装保留输入 NUL 检查和原有空结果语义，
不再经过 `point`/`takeCstr` 转发。Windows 默认专项门禁完整通过严格 C、
独立 oracle、21 个生成原型、JIT/AOT、双语范例及 DLL 宿主复用；
Linux 包级门禁通过 C ASan/UBSan、线程、语言 JIT/AOT 和同输出范例。
生成 C 的原生调用直接填入字符串数据指针，仍需一次长度扫描；
本次只消除中间转发，不宣称整个函数调用与手写 C 等速。

十六进制 RGB/RGBA、HSV/HSL 转换、命名色和对比度工具。C 核心只写调用者缓冲；
xlang 结果独立拥有。没有共享可变颜色缓存，数值查询和 C 核心正常路径不分配。

```xlang
import color
string accent = color.normalize("#36c")
println(accent)
println(color.mix("#000", "#fff", 0.5))
```

输出依次为 `#3366CC`、`#808080`。

## 输入与返回规则

- 十六进制输入为 3、4、6、8 位，`#` 可省略，字母不区分大小写。不接受前后空白。
- 四位/八位的 alpha 位于末尾；没有 alpha 时按 255 处理。
- `valid("red")` 为 false。名称输入须先经过 `fromName`，不隐式猜测输入格式。
- 非法颜色返回 false、-1 或空字符串，这是正常查询结果；内存、输出缓冲参数错误另算。
- `normalize`、`mix` 返回不带 alpha 的 `#RRGGBB`，不是透明度合成操作。
- 这是有限的颜色工具，不是完整 CSS 解析器，不解析 `rgb(...)`、`hsl(...)`、色域或系统颜色。
- C 输入是借用的 NUL 结尾文本，不保留输入指针。xlang 的公开字符串入口会在进入
  C 桥接前检查内嵌 NUL，并按非法输入返回 false、-1 或空字符串，不解析有效前缀。

## 全部公开函数

下表中的 `text/a/b/name` 为 string，RGB/alpha 分量为 int，`h/s/v/l/t` 为 float。
没有可选参数，所有参数都应显式提供。

| 调用 | 返回 | 含义 |
| --- | --- | --- |
| `valid(text)` | bool | 是否为可解析的十六进制 RGB/RGBA |
| `red(text)` | int | 红分量 0..255，非法为 -1 |
| `green(text)` | int | 绿分量 0..255，非法为 -1 |
| `blue(text)` | int | 蓝分量 0..255，非法为 -1 |
| `alpha(text)` | int | 透明度 0..255，非法为 -1 |
| `hex(r, g, b)` | string | 大写 `#RRGGBB`；分量超出 0..255 返回空串 |
| `hexA(r, g, b, a)` | string | 大写 `#RRGGBBAA`；分量越界返回空串 |
| `normalize(text)` | string | 展开短写并输出 RGB，舍弃 alpha；非法返回空串 |
| `hue(text)` | float | 色相角 [0,360)，灰色为 0；非法为 -1 |
| `saturation(text)` | float | HSV 饱和度 [0,1]，不是 HSL 饱和度；非法为 -1 |
| `value(text)` | float | HSV 明度 [0,1]；非法为 -1 |
| `lightness(text)` | float | HSL 亮度 [0,1]；非法为 -1 |
| `fromHsv(h, s, v)` | string | HSV 转为 RGB；参数规则见下文 |
| `fromHsl(h, s, l)` | string | HSL 转为 RGB；参数规则见下文 |
| `luminance(text)` | float | sRGB 相对亮度 [0,1]；非法为 -1 |
| `contrast(a, b)` | float | 对比度 [1,21]；任一非法为 -1 |
| `isDark(text)` | bool | 合法且相对亮度小于 0.5 |
| `isLight(text)` | bool | 合法且相对亮度不小于 0.5 |
| `mix(a, b, t)` | string | 编码后的 RGB 分量线性插值，结果取最近整数 |
| `fromName(name)` | string | 148 个 CSS 命名色，ASCII 大小写不敏感；未知为空串 |
| `toName(text)` | string | 不透明颜色的精确名称；无匹配或 alpha 非 255 为空串 |

## 数值和名称合同

有限色相按 360 度环绕；HSV 的 s/v 和 HSL 的 s/l 截至 [0,1]。
`mix` 的有限 t 同样截至 [0,1]。任何 NaN 或无穷浮点参数都返回空串，
不会进入浮点转整数的不确定行为。RGB 构造器采用拒绝越界，不采用截断。

`mix` 不是线性光空间插值；`luminance`、`contrast` 忽略 alpha，使用透明颜色前
应由调用方先确定实际背景。`isDark/isLight` 是亮度 0.5 的方便判断，不能代替
前景/背景的对比度计算。示例中的 AA/AAA 只指普通文字的对比度阈值，不是整页认证。

命名色依据 [CSS Color 4 命名色表](https://www.w3.org/TR/2026/CRD-css-color-4-20260926/#named-colors)。
`transparent`、`currentColor`、空白修剪和模糊名称不在本 API 范围内。
同值别名返回按字母顺序排列的第一个名称，例如 aqua、fuchsia、gray；不承诺恢复输入别名。
亮度公式依据 [W3C sRGB 相对亮度定义](https://www.w3.org/WAI/WCAG21/Understanding/relative-luminance.html)。

## C 接口

包含 `include/color.h`，链接 `color_core.c` 和提供所需符号的 XRT。
数值接口返回 bool/int64_t/double；八个文本接口接受 `char *output, size_t capacity`，
容量包含结尾 NUL。`XCOLOR_HEX_SIZE` 为 10，`XCOLOR_NAME_SIZE` 为 24。

```c
char output[XCOLOR_HEX_SIZE];
bool ok = xcolor_hex(51, 102, 204, output, sizeof(output));
/* ok 为 true，output 为 #3366CC；output 是调用者自己的数组。 */
```

- 每次公开调用清除旧 XRT 错误。无效颜色使文本接口返回 false 并输出空串，不设置错误。
- NULL 输出或零容量设置 XERR_ARGUMENT；容量不足设置 XERR_RANGE，非零容量时输出置空。
- 输出足够大时允许与输入缓冲重叠。各线程使用自己的输出缓冲，库不共享可变结果。
- C 核心无需释放结果。仅 `include/color_native.h` 中的 `*_text` 语言适配入口
  返回 XRT 拥有的 `char*`，C 使用者须 `xrtFree`，xlang 通过 `@owned` 直接接管。
  这些入口的 NULL 表示分配失败，不是非法颜色的空结果。

不需要调用语言桥时，只链接 C 核心即可，不必把 xlang 编译器带入 C 程序。

## 双语范例与测试

2026-10-26 完整重建 Windows `xl.exe`/`xl.dll`/`xlc.dll` 和 Linux
`xl`/`libxl.so`/`libxlc.so` 后复验：Windows 默认门禁 `passed-scoped`，
43 项检查通过、输入哈希不变；Linux 严格 C ASan/UBSan/float-cast、四线程
8000 轮、26552 项独立 oracle 与语言 JIT/AOT 全通过。Linux 门禁新增
中央语言用例和 C 单文件范例的独立 EXE；后者与 GCC C、TCC C 及
XLang 基础范例逐字节同输出。两平台的测试目录现在默认独占，Linux
默认临时产物退出时清理；显式 `COLOR_WORK_DIR` 可保留验证证据。
这次复验时，八个拥有型 C 文本结果仍由语言层 `takeCstr` 直接接管，
不增加复制；2026-10-27 的直接接管迁移见文首。
本包在上述 Windows/Linux x64 范围签收。脚本仍明确标记
`releaseReady=false`：它只运行 color 专项门禁，不代表整个产品的回归
或发布准入。32 位/ARM、TSan 与长时性能不在此次包级签收范围。

2026-10-25：独立 oracle 新增 4101 组 `contrast` 亮度比及 4131 组
`mix` 编码 RGB 插值对照，含短/长 alpha、端点、截断参数和固定种子颜色对；
与原有转换和命名色合计 26552 项。Windows 严格 C/线程/oracle 均通过，
当日 Windows 语言阶段曾受根目录 VFS 头路径阻断；当前发布物已通过；
Linux C ASan/UBSan/float-cast、XLang JIT 与双语门禁全部通过。
同日使用当前 Linux 发布版补跑边界、分配故障、基础范例和调色板范例的
独立 EXE AOT，四条路径全部通过；基础范例与 C 黄金输出一致，调色板
范例与 JIT 输出一致。本轮未改颜色实现。

- `release/examples/extlibs/color_basic.xl` 与本包 `examples/basic.c` 覆盖全部 21 个 API，
  实现同一组颜色转换及黑/白前景选择，输出与固定的 expected 文件逐字比较。
- 原有 `release/examples/extlibs/color.xl` 保留调色板示例。
- 在发行目录运行 `xl run examples/extlibs/color_basic.xl`。
- 在 `demo6` 根目录可直接运行 `release/xl.exe run release/extlibs/color/examples/basic.c`；C 范例只内嵌纯 C 颜色核心，不内嵌 XLang 适配层。
- 在仓库运行 `tools/run_extlib_c.ps1 -Package color -Source release/extlibs/color/examples/basic.c`。

C 范例以栈缓冲和显式条件检查所有调用；两平台门禁均用 `-DNDEBUG`
构建执行，防止 Release 模式跳过颜色转换而打印未初始化结果。

门禁：`tools/check_extlib_color.ps1` 与 `tools/check_extlib_color.sh`。
2026-10-24 使用更新后的 Windows 发布版复验，默认专项门禁完整通过，
包括实际生成原型、语言边界与分配故障 AOT、双语范例 AOT、DLL 上下文复用
和文档代码块。Linux C ASan/UBSan/float-cast、独立 oracle、线程与语言
对照门禁也通过。旧共享文本快照缺符号已解除；脚本仍将结果标为
`passed-scoped` 而非正式产品全量重建，不据此签收其他架构与性能要求。
门禁覆盖独立数值/名称
参照、缓冲边界、重叠输入输出、非有限数、零分配核心、结果寿命、故障注入、线程隔离、
实际生成原型、JIT/AOT、DLL 上下文复用以及 C/xlang 固定输出。名称夹具来自 W3C，
日常门禁不联网，也不从待测色表提取期望值。

当前正式 `xl.exe` 运行 `test/allocations.xl` 已通过，不再把旧的共享异常清理
阻断列为现存问题。Linux 门禁同次运行 C sanitizer/oracle、中央与包内
XLang 边界及分配故障，并将双语范例与黄金输出对照。
32 位/ARM、TSan 和长期性能尚未验证。
完整执行证据与剩余边界记录在 `demo6/docs/扩展库Color限定交付_2026-09-28.md`。

以下为历史验证记录，其中 AOT 阻断描述针对当时的旧发布版。

2026-10-17：`tools/check_extlib_color.sh` 已在 Linux 跑通 C ASan/UBSan/
float-cast、独立数值 oracle、线程测试、XLang 中央与包内边界、故障注入
及 C/XLang/黄金输出对照。9 月限定交付记录中的 Linux XLang 与正式 CLI
分配失败待办已由当前发布物复核覆盖；完整 AOT 共享阻断不变。

2026-10-19：C 用例新增 1024 组确定性 RGBA 组合，XLang 用例新增 256 组；
每个通道均覆盖 0..255，检查 `hexA` 编码后逐通道解析，以及去除 alpha 后
`normalize` 与 `hex` 的结果一致。这是逐通道全字节覆盖，不是 2^32 种
RGBA 组合的穷举。Windows `-SkipAot` 门禁及 Linux C ASan/UBSan/float-cast、
独立数值参照、线程、XLang 边界与双语范例门禁均通过。实现未改，完整 AOT
仍受共享缺符号阻断。

2026-10-21：两份 C 测试原将颜色转换、分配故障和线程操作放在
`assert(...)` 表达式里；定义 `NDEBUG` 会把调用一并删掉。测试单元现在
显式保留断言，Windows/Linux 门禁均以 `-DNDEBUG` 编译并运行这两份
测试。预处理核对关键调用仍在；Windows 的 21 个 API、独立 oracle、
线程与双语门禁，以及 Linux C ASan/UBSan/float-cast、8000 轮线程和
XLang 对照均通过。库本体/API 未改，完整 AOT 的共享阻断仍未解除。

2026-10-23：语言分配故障测试的单名 `catch MemoryError` 原是无类型
捕获变量，无法证明异常类别；现改为 `catch MemoryError error`。
八个公开颜色入口的故障扫描与恢复、Windows `-SkipAot` 专项和 Linux
ASan/UBSan/float-cast、独立 oracle、线程及语言联合门禁通过。实现和
公开 API 不变；完整 AOT 仍受共享缺符号阻断。
