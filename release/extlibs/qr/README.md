# qr

2026-10-27 混合模式收口：容量规划改为在数字、字母、字节三种片段间
按实际位数选择最短分段；`version()` 与真正写入的片段使用同一规划。
40 位数字 + 小写字母 + 40 位数字在 M 级从整段 byte 模式所需的
较大版本降到版本 3；夹入 UTF-8 中文的样本为版本 4，写出一次
ECI 26。Windows/Linux 默认包门禁通过严格 C、Linux ASan/UBSan、
XLang JIT/AOT、双语范例及独立 EXE；独立 ZXing-C++ 3.1.1 对
新增混合样本和既有长度边界均成功解码。不扩展输入长度上限或
字符集，不宣称专业印刷质量认证。

2026-10-27：三种渲染的 C 结果都由 XRT 分配，语言声明统一使用
`@owned string?` 直接接管；移除 `point/takeCstr` 手工交接，保留原有
空结果错误处理。C 头文件同步将结果类型写为 `char*`，分配和释放
约定不变。生成 C 直接把结果指针赋给字符串数据，仅调用一次
`strlen` 确定长度，没有复制正文。连续渲染与失败后，旧字符串仍
独立有效。Windows 完整包级门禁通过；Linux C ASan/UBSan、语言
JIT/AOT 和 C/XLang 输出对照均通过。

生成标准 QR Code：版本 1-40、L/M/Q/H 四种纠错级别、数字/字母/UTF-8 字节模式、Reed-Solomon 纠错及 8 种掩码选择。非 ASCII 文本写入 UTF-8 ECI 26。编码器按版本组计算最短位数，自动组合数字、字母和字节片段；`version()` 与矩阵生成共用这套规划。文本仍按 UTF-8 原始字节编码，不做汉字专用模式。

```xlang
import qr
var modules = qr.matrix("HELLO WORLD")
string image = qr.svg("你好，世界", "M", 4, 4)
println(qr.version("HELLO WORLD"))  // 1
```

## API

- `matrix(text, level = "M")` 返回行数组，每个元素为 0 或 1；矩阵不含静区。
- `version(text, level = "M")` 只进行容量估算，不生成矩阵。
- `ascii(text, level = "M", dark = "##", light = "  ")` 返回文本二维码，静区为 4 模块。自定义格子须为无控制字符和行分隔符的单行 UTF-8 文本；不同终端的字符显示宽度由调用者负责。
- `utf8(text, level = "M")` 返回半块字符二维码，静区为 4 模块。
- `svg(text, level = "M", border = 4, scale = 4)` 返回可直接显示的 SVG。

`text` 必须是非空合法 UTF-8，不能包含 NUL；最大输入为 7089 字节，实际可编码长度取决于模式与纠错级别。`level` 取 L/M/Q/H，不区分大小写。SVG `border` 为 0..64，`scale` 为 1..1024；ASCII 输出超过 4 MiB 时显式失败，不截断。

C API 见 `include/qr_native.h`。`_n` 接口显式接收 UTF-8 字节长度；矩阵返回值用 `xrtValueRelease`，ASCII/UTF-8/SVG 独立字符串用 `xrtFree`。三种渲染均不使用共享结果缓冲。每次公开调用先清除当前线程上一次的 XRT 错误，调用方可按本次调用的返回值和错误状态判断结果。

## 范例与验证

2026-10-26：当前 Windows 与 Linux 默认包门禁均退出 0。
严格 C/ABI、Linux ASan/UBSan、语言中央/边界 JIT、核心/边界/
中央/公开范例独立 EXE 以及公开 C 范例的 GCC、`xl run`、
`xl build` 输出对照通过。独立 ZXing-C++ 3.1.1 解码器还读回
当前 C 编码器生成的 11 组样本，含中文和容量边界；其 Python
依赖仅用于测试，不进入包。下方旧 VFS/共享符号阻断记录不再
描述当前发布版状态。

2026-10-25：使用当前包源码重新严格编译 C 编码器，并由独立
ZXing-C++ 3.1.1 扫码器读回 11 组矩阵；中文、补充平面字符、
2953 字节 byte 模式与 7089 位数字边界均通过，2954 字节 byte 模式
输入被拒绝。新 `test/check_decode.py` 将编译、扫码和临时产物清理
收为一条可复跑命令；本轮没有借此签收当前受 VFS 头文件阻断的
XLang JIT/AOT 路径。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 C/ABI、C 边界、中央与包内 XLang JIT、C/XLang 同输出、核心、
包内边界、中央用例和带输出范例 AOT 全部通过。旧的索引/文本快照
缺符号记录属于先前发布物。本次本机 Python 环境缺少可选
`zxingcpp`，未重跑独立扫码；此前十一组 ZXing 解码结果仍按其
原验证时间记录，不能当成本次门禁的一部分。

2026-10-22：半块 UTF-8 渲染改用已知的 1/3 字节长度，不再逐格扫描固定字符。C 边界测试按版本 1 矩阵逐格核对四种半块字符、静区、换行与末行；Windows 严格 C、XLang 边界/双语范例和无输出核心 AOT，以及 Linux C ASan/UBSan、XLang/双语门禁通过。完整带输出 AOT 的共享链接阻断未在本轮复验，不签收整包发布级。

- Windows 包级门禁从 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/qr/test/check.ps1 -SkipAot`；去掉 `-SkipAot` 可检查全部 AOT 阶段并报告首个阻断。
- Linux 原生 C sanitizer、XLang 中央/包内边界及 C/XLang 范例逐字对照门禁从 `demo6` 目录运行 `bash release/extlibs/qr/test/check.sh`。
- `release/examples/extlibs/qr.xl` 与 `qr_c.c` 可直接 `xl run`，输出一致。
- `release/extlibs/qr/test/test_qr.c` 与 `test/boundaries.xl` 覆盖格式、所有权、超长、无效输入和大矩阵。
- `test/aot_core.xl` 只覆盖无输出的公开 `version` 路径，用于在完整 AOT 受主线阻断时验证本包的依赖裁剪；不代替矩阵或渲染的 AOT 验收。
- `test/dump_matrix.c` 与 `test/verify_decode.py` 使用独立 ZXing 解码生成矩阵；此附加验证需要测试环境安装 `numpy`、`zxing-cpp`，不是运行时依赖。覆盖 v1、中文、长文本与 v40 的 7089 位数字。
- 安装上述测试依赖后，在 `demo6` 目录运行 `python release/extlibs/qr/test/check_decode.py`；脚本以严格 GCC 编译当前 C 源，在独占临时目录执行扫码测试，不写入发布目录。

2026-10-12：Windows 严格 GCC、Linux ASan/UBSan、C/XLang 边界、双语同输出与独立扫码通过。当时的无输出边界 AOT 曾通过；当前发布版必须按下方复核结果重新验收。

2026-10-14：自定义明暗格原允许换行、ESC 和 Unicode 行分隔符，导致文本矩阵损坏；C/XLang 红绿边界后改为按 XRT UTF-8 标量拒绝 C0/C1、DEL、U+2028/U+2029。C 入口失败后成功仍留旧错误的回归先红后绿；Windows 严格 GCC、C/XLang 边界与双语范例、包内 Linux ASan/UBSan 门禁通过；编码核心和独立解码基线未改。当前 `xl build test/cases/extlib_qr.xl -o build/test/qr_core.exe` 缺 `xl6ValueIndexReadCopyV1`；带输出范例另受 `xl6ValueDataSnapshotV1` 阻断。两者均为共享 AOT 链接问题，见 `demo6/dev/extlib_blockers/value_index_link.md` 和 `text_snapshot_link.md`；尚不能签收发布级 AOT。

2026-10-15：清单声明原生代码直接使用的 XRT Buffer、Value Container、Unicode 模块，源码宏条件化；新增只测 `version` 的无输出 AOT 探针，同一程序从 3,559,424 降至 568,832 字节且运行通过。矩阵前 16 个、ASCII 渲染前 8 个分配点的故障注入均保持 `XERR_MEMORY`，Windows C/XLang 边界、双语范例和 Linux ASan/UBSan 复跑通过；完整边界 AOT 仍缺 `xl6ValueIndexReadCopyV1`。本轮未重跑独立扫码解码：当前 Python 环境缺少可选测试依赖 `zxingcpp`，既有解码结果不计入本轮验证。

2026-10-15：新增 Windows 包级 `test/check.ps1`，按 manifest 严格编译、C 边界、中央 XLang JIT、包内边界、C/XLang 范例同输出及核心 AOT 均通过。当前发布物的包内 `test/boundaries.xl` AOT 也已构建运行；上段所述缺索引符号仍在中央 `test/cases/extlib_qr.xl` AOT 复现，不能扩大包内边界结果。完整门禁准确停在中央 AOT 的 `xl6ValueIndexReadCopyV1`；独立构建带输出范例则缺 `xl6ValueDataSnapshotV1`。Linux ASan/UBSan 复跑通过；可选 ZXing 独立解码未重跑。

2026-10-16：从当前源码以严格 GCC 重建矩阵输出夹具，用独立 ZXing-C++ 3.1.1 解码九组固定样本全部通过，含 UTF-8 中文、500/1000/1800 字节高纠错文本及 7089 位数字容量边界。测试依赖仅安装在 `build/test/qr_guard/pydeps`，不进入发布包。同日 Windows 包级 `-SkipAot` 门禁通过 C、XLang、双语输出及无输出核心 AOT；中央完整 AOT 链接阻断不变。

2026-10-16：Linux XLang 导入复现 `version` 的 `long long`/`int64_t` 原型冲突；`version` 返回值及 `svg` 的数值参数统一为 `int64_t`，实现包含公开头，新增 C ABI 门禁。跨平台调用与 C/XLang 范例复验；完整 AOT 阻断不变。

同日将包内 `test/boundaries.xl` 登记为 `coverage.tests`，供目录 smoke
自动发现；当前发布物的该用例和 C 边界测试通过。未重跑独立扫码或
完整 AOT，不因本次登记签收发布级验收。

2026-10-17：Linux 包级脚本加入中央与包内 XLang JIT 用例，并严格比较
C 和 XLang 范例的输出；与 C ASan/UBSan 同次执行。完整 AOT 和可选的
独立 ZXing 解码不在本轮 Linux 脚本内，不因此签收发布级。

2026-10-18：使用临时目录中的 ZXing-C++ 3.1.1 重新运行独立解码；
测试向量从九组扩至十一组，新增补充平面 Unicode 字符和 byte 模式
2953 字节容量边界，2954 字节输入明确拒绝。十一组二维码均由独立
解码器读回原文。测试脚本限制 NumPy/OpenBLAS 为单线程，仅影响可选
验证进程。该结果不覆盖所有模式组合，也不解除共享 AOT 链接阻断。
