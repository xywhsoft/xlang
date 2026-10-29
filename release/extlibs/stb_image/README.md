# stb_image

2026-10-26 当前 Windows/Linux 发布版的默认包门禁均通过原生 C、
语言 JIT、核心与公开 XLang 范例 AOT；新增公开 C 范例的独立 EXE，
借显式图片路径使用同一夹具，GCC、C/AOT 与 XLang JIT/AOT 输出一致。
Linux 保留 C ASan/UBSan 与线程回归。两平台最后的包外
`StbImage(point)` 私有构造器负例仍被编译器接受为 `ok`，因此
门禁保持红色，完整发布级尚未签收。原生类实现与公开 API 未改。

`stb_image` 将 stb_image 2.30 的图片解码能力以 C/XL 双语接口公开。
文件读取交给 XRT，因此 Windows 上的 UTF-8 中文路径与其他平台使用
同一入口；stb 的 stdio 代码未编进包。也支持直接解码 `Buffer` 中的
PNG/JPEG/GIF/BMP/TGA/PSD/PIC/HDR 等 stb_image 支持的格式，具体格式
边界以所附 `third_party/stb_image.h` 为准。

```xlang
import stb_image

stb_image.StbImage image = stb_image.load("photo.png", 4)
println(image.width(), image.height(), image.channels())
Buffer pixels = image.pixels()
```

`StbImage` 是自动析构的 native-backed class。`width()`、`height()`、
`channels()`、`valid()`、`isFloat()`、`byteSize()` 返回解码后的属性；
原生整数状态入口 `validNative()`、`floatNative()` 仅供类内包装使用，
不作为包外 API。
接管句柄的 `StbImage(point)` 已声明私有，但当前编译器仍接受包外调用；
见 `demo6/dev/extlib_blockers/stb_image_private_adopt_constructor.md`。
主线修复前，本包不能签收完整发布级访问安全。
2026-10-24 当前 Windows 发布版复验：严格 C、线程、XLang 边界与分配故障、
C/XLang 双语输出，以及核心和公开范例独立 EXE 均通过；旧的 AOT 缺符号
故障未复现。但仅编译负例
`xl check release/extlibs/stb_image/test/private_adopt_constructor_negative.xl`
仍返回 `ok`，现已列入默认门禁并使其明确失败。此文件只用于编译检查，
绝不能执行。主线需在导出类的构造器重载解析中拒绝包外调用
`private extern create(point)`，同时保留包内工厂接管；修复后重跑完整门禁。
`pixels()` 返回独立的 `Buffer` 副本。`data()` 返回借用的原生指针，
只在图像对象存活期间有效。`loadf`/`loadfBytes` 生成 32 位浮点通道；
`loadBytes`/`loadfBytes` 读取内存图片。`info(path)` 返回
`{width,height,channels}` JSON 文本，不完整解码像素；`isHdr(path)`
对畸形文件报错，不把它误判为有效 LDR 图片。

编码文件最多 128 MiB，解码像素最多 256 MiB；通道请求只接受 0 到 4，
其中 0 保持源通道数。超过上限、无效通道、文件 I/O 和畸形图片都通过
XRT 结构化错误传到 XL 异常，不返回可误用的空句柄。XLang 入口在转换为
C `int` 前检查通道数，并拒绝路径内嵌 NUL，防止整数或路径截断后误读。

C 接口见 `include/stb_image_native.h`。C 调用者对返回的图像句柄使用
`xstbi_image_release`，对 `xstbi_info_json` 返回的文本使用 `xrtFree`；
输入内存仅借用。stb 的解码分配、扩容与释放都接入 XRT，内存不足
通过 XRT 错误通道报告；探测格式时某个临时分配失败但最终成功的调用
不留下旧错误。包内保留 stb 的许可说明 `LICENSE.stb.txt`。

包级验证：

- `powershell -NoProfile -ExecutionPolicy Bypass -File extlibs/stb_image/test/check.ps1`
  默认运行严格 GCC、C/XL 边界、多线程、固定输出与双语范例检查，
  然后构建运行核心、XLang 范例与 C 范例 AOT，最后检查包外访问边界；
  `-SkipAot` 仅做 C/JIT 聚焦快测，仍检查访问边界。
  AOT 产物放在独占系统临时目录，失败时同样清理，不把快测通过当作
  发布级签收。
- Linux 上运行 `bash release/extlibs/stb_image/test/check.sh`，执行 C 边界与
  多线程的 ASan/UBSan 检查、XLang 中央与包内边界，并逐字节比较
  C/XLang JIT 和独立 EXE 范例输出；最后检查包外构造器访问。
- 在 `release` 目录运行 `xl run examples/extlibs/stb_image.xl` 或
  `xl run examples/extlibs/stb_image_c.c`，两者输出一致。C 范例直接包含
  包实现，无需额外列出源文件；两个范例都从 `xl.exe` 所在目录定位素材，
  从仓库根目录调用发布版 `xl.exe` 也可运行。XLang 范例可在 `--`
  后指定图片绝对路径，供独立 AOT 可执行文件使用；C 范例将路径
  作为第一个命令行参数传入。
- 从仓库根目录运行 `xl run release/extlibs/stb_image/test/boundaries.xl`，
  包括通道数回绕及路径内嵌 NUL 负例；该用例已登记到包级测试套件。
- 用 `tools/run_extlib_c.ps1 -Package stb_image -Source ...` 运行
  `test/test_stb_image.c` 和 `test/test_stb_image_threads.c`。
- C 测试包含 UTF-8 中文路径、文件/内存像素一致性、浮点通道、非法输入
  与四线程 800 次解码，并逐点注入整数/浮点解码分配失败；Windows GCC `-Wall -Wextra -Werror`、
  Linux ASan/UBSan 已通过。

2026-10-15：原 stb 内部仍使用 CRT 分配，XRT 故障注入只能覆盖外层图像句柄；
新增 C 扫描先红后绿，现统一使用 XRT 分配器。扫描还复现格式探测在
内部申请失败后成功恢复、却保留 `XERR_MEMORY` 的情况；成功返回路径
已清理旧诊断。Windows 包级门禁、Linux ASan/UBSan 与双语范例通过。

历史阻断：旧发布物曾因缺少 `xl6NativeHandleTraceLeaf` 或
`xl6ValueDataSnapshotV1` 无法完成 AOT，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`。当前 Windows 发布物
已通过核心和公开范例 AOT；剩余发布阻断是私有句柄构造器访问控制。

2026-10-17：Linux 包级门禁补正式 `release/xl` 的中央和包内 XLang 用例，
以及 C/XLang 范例逐字节同输出检查；与 C ASan/UBSan 边界和线程测试
同次通过。完整 AOT 阻断不变。

2026-10-18：包外调用两个原生状态方法的只编译负例原返回 `ok`；现将
它们设为私有，公开 `valid()` / `isFloat()` 保持不变。Windows 严格 C、
线程、XL 与双语范例，以及 Linux ASan/UBSan/XL 门禁通过；私有指针接管
构造器的主线访问控制缺陷和完整 AOT 阻断仍未解决。

2026-10-23：XLang 新增文件/内存、整数/浮点四种加载入口的逐点分配故障扫描，
验证真实失败以 `MemoryError` 传递、内部恢复后仍能返回有效图像，以及每次
解除故障后可继续解码。测试已纳入 Windows/Linux 包级门禁和包清单；C 层
已有的故障注入保持不变。Windows 严格 C、边界、线程与双语对照，以及
Linux C ASan/UBSan、XLang 和双语对照均通过；既存主线阻断仍未解除。
