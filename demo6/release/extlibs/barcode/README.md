# barcode

2026-10-25 再次使用当前发布物执行 Windows 默认 `test/check.py`：
严格 C 语法检查通过，首个 XLang 中央用例从 `demo6` 根目录运行时
因共享 TCC VFS 缺少 `include/xruntime_decl.h` 停止；后续边界、双语
范例及 AOT 阶段未执行。同一中央用例从 `release` 目录运行通过，
包内 C 边界测试通过。下方早先的完整通过记录只对应当时发布物，
不能替代本次验收；见
`../../../dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`。
包内不加头文件路径兼容层。

EAN-13 与 UPC-A 条码编码。输入 12 位 EAN 数据或 11 位 UPC 数据时自动计算校验位；输入完整 13/12 位编码时严格校验。生成 95 个 0/1 模块，也可渲染为 ASCII 或不依赖外部文件的 SVG。本包不提供 UPC-E、Code 128、印刷尺寸认证或扫码识别。

```xlang
import barcode

var modules = barcode.upcA("12345678901")  // 自动补成 123456789012
assert modules.length == 95
println(barcode.upcAAscii("12345678901"))
string svg = barcode.ean13Svg("4006381333931", 60, 3)
```

`ean13` / `upcA` 返回 95 项数组；`ean13Ascii` / `upcAAscii` 返回 95 字符的明暗表示。`ean13Svg` / `upcASvg` 的 `height`、`scale` 必须为 `1..1000000`，非法值抛 `ValueError`，不静默夹取或截断。`checksumEan` 只接受 12 位 EAN 数据，非法输入返回 `-1`；`validateEan` 只接受 13 位 EAN 编码，非法输入返回 `false`，不会抛异常。生成和渲染接口对错误长度、非 ASCII 数字、内嵌 NUL 与校验位不匹配抛 `ValueError`。

SVG 在 95 个编码模块外保留白色静区：EAN-13 左 11X、右 7X；UPC-A 左右各 9X，因此两者画布宽度都是 `113 * scale`，首条黑纹分别从 `11 * scale`、`9 * scale` 开始。原始模块数组和 ASCII 输出仍只含 95 个编码模块，不把静区混进数据。静区尺寸依据 [GS1 General Specifications](https://ref.gs1.org/standards/genspecs/)；实际印刷还须按使用场景检查 X 尺寸、条高和质量等级，本包不自动保证印刷认证。

合法输入在编码或渲染时若分配失败，抛 `MemoryError`；失败后可再次调用。C 入口保留 XRT 内存错误，不将其改写成输入错误。

## C 接口

公开头文件为 `include/barcode_native.h`。字符串入口同时传入指针和真实字节长度；不以 `strlen` 截断输入。`xb_ean13` / `xb_upc_a` 返回的 `xvalue*` 由调用方使用 `xrtValueRelease` 释放。`xb_svg` / `xb_ascii` 返回独立 XRT 内存，调用方用 `xrtFree` 释放。布尔校验返回 `bool`，与 XLang extern ABI 一致。渲染结果不使用共享静态缓冲区。

每个公开 C 入口开始时清理本线程的旧 XRT 错误。无效编码使生成/渲染入口报告 `XERR_VALUE`；正常校验查询返回 `-1` 或 `false`，不保留上一次失败的错误；成功后也没有旧错误残留。分配失败仍报告 `XERR_MEMORY`。

## 验证

2026-10-25：C 边界新增 EAN-13 首位 0..9 的全部左半区 L/G 奇偶模式校验；
其余数据位固定为零，逐位核对六段七模块编码。原有完整 95 位黄金向量
继续约束实际条码位序。Windows 默认 `test/check.py` 的严格 C、
语言边界、双语固定输出及核心/范例 AOT 全通过；Linux `test/check.sh`
的 C ASan/UBSan、语言边界和双语门禁也通过。实现及公开 API 未变。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.py`，
严格 C 语法、C/XLang 边界、故障恢复、分配失败、C 文件直接运行、
双语固定输出及核心/完整范例 AOT 全部通过。此前完整范例 AOT 的
共享缺符号记录属于旧发布物；本次未重跑 Linux sanitizer，也不把
这项包级结果解释为条码印刷认证。

从 `demo6` 目录运行：

```powershell
python release\extlibs\barcode\test\check.py
python release\extlibs\barcode\test\check.py --skip-aot
.\release\xl.exe run test\cases\extlib_barcode.xl
.\release\xl.exe run test\cases\extlib_barcode_boundaries.xl
.\release\xl.exe run release\extlibs\barcode\test\recovery.xl
.\release\xl.exe run release\extlibs\barcode\test\allocations.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package barcode -Source release\extlibs\barcode\test\test_barcode.c
.\release\xl.exe build release\extlibs\barcode\test\aot_core.xl -o build\test\barcode_aot_core.exe
.\build\test\barcode_aot_core.exe
.\release\xl.exe run release\examples\extlibs\barcode.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package barcode -Source release\extlibs\barcode\examples\barcode.c
```

C/XL 测试把 UPC-A 短码与全码的全部 95 个模块，逐位对照等价的 `0` 前缀 EAN-13；覆盖错误校验、内嵌 NUL、极限 SVG 尺寸、独立结果所有权。Windows 严格 GCC、Linux ASan/UBSan 和无输出 AOT 核心已通过。带输出范例的 AOT 构建仍被共享主线缺符号 `xl6ValueDataSnapshotV1` 阻断，详见 `demo6/dev/extlib_blockers/text_snapshot_link.md`，发布级验收暂不签收。

C 教学范例 `examples/barcode.c` 也可直接用 `xl run` 单文件运行；仅此路径在 TCC 下编入原生实现，普通 C 构建继续按包清单分离链接。Windows/Linux 门禁分别核对单文件、分离构建和 XLang 范例的输出。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/barcode/test/check.sh`，重建严格 GCC 与 ASan/UBSan C 测试，运行四组 XLang JIT 用例并逐字节比较 C/XLang 范例输出；临时产物不写入发布目录。2026-10-14 新增编码数组、SVG、ASCII 的逐点分配失败和恢复检查，以及 XLang 的四条入口内存异常测试；编码算法未变。

2026-10-15：C 范例现在与 XLang 范例执行同一组校验、编码及渲染操作，`test/check.py` 在 Windows 上逐行比较两者输出，并运行 C/XLang 边界与无输出 AOT 核心。Python 仅为该门禁所需，不是包的运行依赖。manifest 已按原生源码的实际调用列出 `VALUE_CONTAINER`、`BUFFER`、`STRING`；同一 AOT 核心由 3,585,536 降到 612,864 字节且运行通过。Linux ASan/UBSan C 门禁也已复跑通过。带输出 AOT 的共享主线阻断仍未解除。

2026-10-14 补 SVG 的 GS1 静区：C/XLang 分别验证 EAN-13 与 UPC-A 的画布、首末黑纹坐标和最大 `scale`，95 位模块及 ASCII 输出不变。Windows 严格 GCC、C/XLang 边界、无输出 AOT 核心与 Linux ASan/UBSan 复跑通过；带输出 AOT 的主线缺符号阻断不变。

2026-10-16：将包内失败后恢复和分配故障两条 XLang 用例登记为
`coverage.tests`。当前发布物的四条 XLang 用例和 C 测试均通过；
完整 AOT 仍受共享主线缺符号阻断，不签收发布级。

2026-10-20：Windows `test/check.py` 的核心 AOT 产物改为每次门禁独占的
临时目录，并在结束时清理；不会覆盖 `build/test/barcode_aot_core.exe` 或
误用旧产物。当前构建单次及两份并行门禁均各自通过 12 项检查。

2026-10-17：按 [GS1 EAN/UPC 结构](https://ref.gs1.org/standards/genspecs/)和
[ZXing 奇偶/位宽表](https://github.com/zxing/zxing/blob/master/core/src/main/java/com/google/zxing/oned/EAN13Reader.java)
增加两条完整 95 位黄金向量，覆盖非零首位 EAN-13 与 UPC-A，避免仅用
两个入口互相比对而共同出错。Linux 严格 GCC 首次发现 C 范例缺少
XRT value 容器模块声明；补齐后 C ASan/UBSan、四组 XLang 用例和双语
范例逐字节同输出门禁通过。编码器本身未改，完整 AOT 阻断不变。
同日 Windows `test/check.py` 的 C/XL、范例对照与无输出核心 AOT 全部通过；
单独构建带输出范例仍报未解析的 `xl6ValueDataSnapshotV1`。

2026-10-20：Windows `test/check.py` 增加 `--skip-aot` 包侧快测；默认在
无输出核心之后构建、运行带输出范例 AOT，并与 C 范例逐字比较。
快测通过，默认门禁核心 AOT 通过，完整范例构建在共享
`xl6ValueDataSnapshotV1` 缺符号处正确失败；独占临时目录已清理。
此前仅核心 AOT 成功的默认全绿不能作为发布签收。

2026-10-22：双语范例增加固定预期输出 `examples/barcode.expected.txt`；Windows
`test/check.py --skip-aot` 和 Linux `test/check.sh` 均分别核对 C 分离构建、
C 单文件运行及 XLang 范例，不再只靠双语输出互相比对。两平台聚焦门禁已
通过；带输出 AOT 的共享主线缺符号阻断仍未解除。
