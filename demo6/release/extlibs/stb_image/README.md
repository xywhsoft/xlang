# stb_image

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
  一次运行严格 GCC、C/XL 边界、多线程和双语范例一致性检查。
- Linux 上运行 `bash release/extlibs/stb_image/test/check.sh`，执行 C 边界与
  多线程的 ASan/UBSan 检查、XLang 中央与包内边界，并逐字节比较
  C/XLang 范例输出。该门禁不覆盖完整 AOT。
- 在 `release` 目录运行 `xl run examples/extlibs/stb_image.xl` 或
  `xl run examples/extlibs/stb_image_c.c`，两者输出一致。C 范例直接包含
  包实现，无需额外列出源文件；两个范例都从 `xl.exe` 所在目录定位素材，
  从 `demo6` 根目录调用发布版 `xl.exe` 也可运行。
- 从 `demo6` 目录运行 `xl run release/extlibs/stb_image/test/boundaries.xl`，
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

XL AOT 当前因主线缺少 `xl6ValueDataSnapshotV1` 而失败，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`；尚未完成发布验收。

2026-10-17：Linux 包级门禁补正式 `release/xl` 的中央和包内 XLang 用例，
以及 C/XLang 范例逐字节同输出检查；与 C ASan/UBSan 边界和线程测试
同次通过。完整 AOT 阻断不变。

2026-10-18：包外调用两个原生状态方法的只编译负例原返回 `ok`；现将
它们设为私有，公开 `valid()` / `isFloat()` 保持不变。Windows 严格 C、
线程、XL 与双语范例，以及 Linux ASan/UBSan/XL 门禁通过；私有指针接管
构造器的主线访问控制缺陷和完整 AOT 阻断仍未解决。
