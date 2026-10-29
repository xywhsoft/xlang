# stb_image_write

2026-10-26 重建 Windows/Linux x64 发布物后，两平台默认包门禁
均退出 0：严格 C、Linux ASan/UBSan、尺寸/路径/分配故障与
64 MiB 输出限额、C/XLang 五格式文件逐字节对照，以及核心和
双语范例独立 EXE AOT。C 范例现接受显式素材路径，独立产物
无需与发布目录共址。旧共享 AOT 缺符号不再复现。本包只签收
有界 Buffer 编码/原子写出范围，不替代 `stb_image` 私有构造器
访问控制修复，也不承诺断电持久性或其他架构。

2026-10-25 历史记录：当时在 WSL 中运行 `test/check.sh` 全通过：严格 GCC、
C 边界 ASan/UBSan、XLang 边界/分配故障/输出限额，以及 PNG、JPEG、
BMP、TGA、HDR 五份 C/XLang 范例输出逐文件字节对照。本次语言用例
使用现有 Linux `release/xl`；未重建 Linux 发布版或运行 Linux
独立 EXE，不能据此签收 Linux AOT。

2026-10-24 当前 Windows 发布版默认 `test/check.ps1` 全通过：严格
GCC、C/XLang 边界、分配故障与输出限额、核心及公开范例 AOT。
C、XLang JIT 和独立 EXE 的 PNG/JPEG/BMP/TGA/HDR 五份输出逐文件
SHA-256 一致。下文共享 AOT 缺符号记录属于旧发布物；本轮未
重建 Linux 发布版并验证其独立 EXE。

`stb_image_write` 提供 PNG、JPEG、BMP、TGA、HDR 图片编码。XLang 输入必须是
`Buffer`，不接受没有长度信息的裸指针；HDR `Buffer` 包含按行紧密排列的
`float32` 通道，其余格式为每通道一个字节。`encode*` 返回独立的 `Buffer`，
`write*` 编码后通过 XRT 同目录临时文件原子替换目标，并返回布尔结果；
写入失败时保留已有目标，不承诺断电后的目录项持久性。Windows 中文 UTF-8
路径与 Linux 路径使用同一接口，不走 stb 的 stdio 文件函数。

```xlang
import stb_image
import stb_image_write

stb_image.StbImage image = stb_image.load("photo.png", 4)
Buffer pixels = image.pixels()
Buffer png = stb_image_write.encodePng(pixels,
    image.width(), image.height(), image.channels())
assert stb_image_write.writePng("copy.png", pixels,
    image.width(), image.height(), image.channels())
```

公开函数：`available`、`version`；`encodePng`/`writePng`、
`encodeJpeg`/`writeJpeg`、`encodeBmp`/`writeBmp`、
`encodeTga`/`writeTga`、`encodeHdr`/`writeHdr`。PNG 的可选 `stride`
为每行字节数，`0` 表示紧凑排列；JPEG 的可选 `quality` 为 1 到 100，
默认 90。宽高必须为正、各不超过 16384；通道数必须为 1 到 4；
像素预算 64 MiB，编码结果预算 128 MiB。无效输入不触碰像素内存，
`encode*` 在 XLang 中抛出 `ValueError`，`write*` 返回 `false`。XLang
入口在转换为 C `int` 前检查宽高、通道数和可选参数范围，避免大整数回绕；
`write*` 还拒绝路径中的内嵌 NUL，避免意外覆盖截断后的前缀路径。
合法输入若编码后超过 128 MiB，C 侧报告 `XERR_RANGE`、XLang `encode*`
抛 `RangeError`，不返回部分图片；实际分配失败保留 `XERR_MEMORY`。

C 使用 `include/stb_image_write_native.h`。`xstbiw_encode` 的返回值是
`xbuffer*`，调用方用 `xrtBufferDestroy` 释放；传入的像素指针仅借用。
原生失败通过 XRT 结构化错误查询。stb 临时缓冲使用 XRT 分配器；PNG
压缩复用 XRT 的 zlib/deflate，避开 stb 内置压缩缓冲在扩容失败时的
不安全路径。PNG 保持标准格式，但编码字节不承诺与旧压缩器相同。
包内的 stb_image_write 1.16 有三处局部修正：禁用 stdio 时仍保留
HDR 回调实现；JPEG 位缓冲使用无符号移位；最终 PNG 缓冲申请失败时
释放已压缩数据。许可证见 `LICENSE.stb.txt`。

公开 C 编码与写文件入口会清除本线程旧 XRT 错误；失败保留本次诊断，成功不保留前一次失败。文件写入若由 XRT 返回具体错误，保留该错误供调用方查看。

包级验证（从仓库根目录执行）：

- `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/stb_image_write/test/check.ps1`：
  Windows 严格 GCC、C 边界、中央与包内 XLang、输出限额和双语范例联合门禁；
  C/XLang 范例的五个实际输出文件分别做 SHA-256 逐一核对。默认还构建运行
  核心与 C/XLang 公开范例 AOT 并对照五个文件；`-SkipAot` 只执行
  C/JIT 聚焦快测。
- `.\release\xl.exe run release/examples/extlibs/stb_image_write.xl`
- `.\release\xl.exe run release/extlibs/stb_image_write/test/boundaries.xl`
- `.\release\xl.exe run release/extlibs/stb_image_write/test/allocations.xl`
- `.\release\xl.exe run release/extlibs/stb_image_write/test/output_limit.xl`
- `tools/run_extlib_c.ps1 -Package 'stb_image_write,stb_image' -Source release/examples/extlibs/stb_image_write_c.c`
- `release` 目录下可直接 `xl run examples/extlibs/stb_image_write_c.c`；
  C 范例直接包含解码和编码实现。C/XL 范例默认从 `xl.exe` 所在目录
  定位输入图片，不依赖启动时的当前工作目录；独立 EXE 可显式传素材路径。
- 同命令改用 `release/extlibs/stb_image_write/test/test_stb_image_write.c`
- `wsl bash release/extlibs/stb_image_write/test/check.sh` 可复跑 Linux 严格 GCC/ASan/UBSan C 测试、中央 API 用例、XLang 边界与 64 MiB 输出限额专项测试，并逐字节核对 C/XLang JIT 与独立 EXE 的五种格式。限额专项仍不进入普通目录 smoke。
- 公开范例默认在各自独立的临时目录写文件并清理；显式指定输出目录时保留
  五种格式。C/XLang 范例均可传第二个参数指定源图片路径，供独立 AOT 运行。
- C/XL 范例的成功文本和五种编码文件一致。C 边界测试覆盖五种格式、往返解码、中文路径、
  参数/尺寸/长度拒绝，并对每种格式扫描前 64 个分配故障点；XLang 对五种
  格式各扫描前 128 点，验证异常分类、成功输出及清除故障后的恢复，边界
  测试另覆盖 C 整数收窄及路径内嵌 NUL。Windows GCC `-Wall -Wextra -Werror`
  与 Linux ASan/UBSan 已通过。

先前发布物曾在普通 `Buffer` 的最小 AOT 缺少
`xl6NativeHandleTraceLeaf`，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`；带输出程序也曾
缺少共享 `xl6ValueDataSnapshotV1`，见 `text_snapshot_link.md`。
2026-10-26 两平台默认包门禁已验证上述缺符号不再复现。

2026-10-15：中央 `test/cases/extlib_stb_image_write.xl` 删除已不存在的 `last_error()` 调用，改验当前 API 的无文件副作用负例；C 测试补失败后成功的错误状态回归并修复原生入口。清单显式声明 XRT Buffer/整文件模块。中央无输出 AOT 构建运行通过（714,240 字节），C/XL 边界、双语范例及新增 Linux ASan/UBSan 门禁通过；带输出完整 AOT 仍缺共享快照符号。

2026-10-15：PNG 内存编码增加逐点 XRT 分配故障扫描，验证故障不返回部分结果、错误可查询，随后正常编码能恢复。Windows C 联合构建、release 工作目录下的 XLang 边界及 Linux ASan/UBSan 门禁通过；编码实现未改，完整 AOT 阻断不变。

2026-10-15：文件写入从截断式 `xrtFileWriteAll` 改为 `xrtFileWriteAtomic`。Linux 以 4 字节文件大小限制先复现写出失败后旧图被破坏，再验证原目标完整保留；Windows 严格 GCC/C、XLang 边界、Linux ASan/UBSan 与裁剪无输出 AOT 构建运行通过。完整带输出 AOT 的共享缺符号阻断不变。

2026-10-15：64 MiB 单通道灰度 BMP 会扩展成超过 128 MiB 的 RGB 输出，原误报内存不足；C 回归先红后绿，现输出限额报告 `XERR_RANGE`，实际分配失败保留 `XERR_MEMORY`。Windows C、XLang 限额/原边界、双语范例和 Linux ASan/UBSan 通过。当前正式发布构建的普通 `Buffer` 最小 AOT 另缺 `xl6NativeHandleTraceLeaf`，见 `dev/extlib_blockers/native_handle_trace_aot.md`；不据旧快照核心 AOT 结果签收本包。

同日追加：PNG 编码原仅有 2 个 XRT 可注入的分配点，内部仍使用 CRT 且 stb 内置压缩缓冲不能安全处理扩容失败。改由 XRT deflate 生成标准 zlib 数据并接管 stb 其余临时分配；PNG/HDR 分配故障扫描与最终缓冲 OOM 清理通过 Windows C 和 Linux ASan/UBSan，XLang 边界、双语范例及 Pillow 独立 PNG 解码通过。AOT 主线阻断未变。

2026-10-16：C 边界测试的中文 PNG 从源码树固定路径移入 XRT 创建的
独立系统临时目录，正常结束时清理目录，避免并行门禁互相覆盖。
Windows TCC C（连同 `stb_image` 测试依赖）及 Linux 严格 GCC、
ASan/UBSan 门禁通过；未重跑完整 AOT。

同日 XLang 边界用例改用 仓库根目录的素材路径和独立系统临时
目录，`finally` 清理测试文件；旧 `release` 工作目录路径先复现
`FileError`，修复后通过。已登记为包的 `coverage.tests`；64 MiB
输出限额测试仍单独运行，不进入默认轻量 smoke。

2026-10-18：Linux 包级脚本补跑中央无文件副作用的 `available/version`
及短像素缓冲拒绝用例；严格 GCC、C ASan/UBSan、XLang 边界/限额和
双语范例联合门禁通过。完整带输出 AOT 的共享阻断不变。

同日 C/XLang 发布范例默认使用各自独立的系统临时目录，结束时清理；
需要保留编码图片时，向范例传入输出目录。两种语言的默认范例均在
仓库根目录运行通过，不会覆盖 `release/examples_out` 里的已有图片。

同日补验：C 范例原仅包含原生声明，直接 `xl run` 缺实现符号；现可
单文件运行。C/XL 双语范例在仓库根目录和 `release` 目录均通过，
GCC 严格语法检查通过；完整 AOT 阻断未变。

2026-10-19：新增 Windows 包级 `test/check.ps1`，将严格 GCC、C 边界、
中央与包内 XLang 边界、64 MiB 输出限额专项，以及 C 直跑、包运行器与
XLang 范例的固定成功输出合并验证；当前门禁通过。编码器未改，Linux
ASan/UBSan 的既有脚本本轮未重跑；完整带输出 AOT 仍未验收。

2026-10-23：发现此前 C 范例只写 PNG，XLang 范例却写 PNG、JPEG、BMP、TGA、
HDR，单靠成功文本一致不足以证明双语等价。现 C 范例同样写出五格式并核对
PNG 文件内容，Windows 门禁按 SHA-256、Linux 门禁用 `cmp` 对五份实际文件
逐一比较。Windows 包级门禁和 Linux C ASan/UBSan、XLang 联合门禁退出 0；
未改编码器或 xlang 主线，AOT 共享阻断仍未解除，不签收整包发布级。

同日补默认 AOT 门禁：Windows `-SkipAot` 聚焦检查退出 0；默认路径在中央
核心 AOT 构建处因共享 `xl6NativeHandleTraceLeaf` 未解析非零退出，后续公开
范例 AOT 尚未执行，独占临时目录无残留。门禁失败不代表编码算法失败，
但在主线阻断解除前不能签收完整发布级。
