# stb_truetype

`stb_truetype` 使用 stb_truetype 1.26 读取 TrueType/OpenType 字体指标并
生成单字形灰度位图。XLang 公开自动析构的 `Font` 和 `Bitmap`：

```xlang
import stb_truetype

stb_truetype.Font font = stb_truetype.load("assets/font.ttf")
stb_truetype.Bitmap glyph = font.bitmap(65, 24.0)
Buffer alpha = glyph.pixels()
println(glyph.width(), glyph.height(), glyph.xoff(), glyph.yoff())
```

`load(path, fontIndex=0)` 使用 XRT 读取 UTF-8 路径；
`loadBytes(Buffer, fontIndex=0)` 复制字体字节，不依赖调用方继续持有输入。
`Font` 提供 `scale`、`ascent`、`descent`、`lineGap`、`glyphIndex`、
`advance`、`leftBearing`、`kernAdvance`、`bitmap`。`Bitmap` 的 `data()`
是只在位图对象存活期间有效的借用指针；`pixels()` 返回独立 `Buffer`。
空白字形合法，宽高和像素长度可为零。`defaultFontPath()` 可查找
常见系统字体；正式程序应随应用提供字体并显式传入路径或字节。

字体文件最多 32 MiB，字形位图最多 64 MiB，字号范围 `(0, 1024]`。
读取前校验 SFNT/TTC 文件头、选中字体索引、表目录和表的文件边界。
XLang 的 `load/loadBytes` 在转换到 C `int` 前检查字体索引，`bitmap`
在转换前检查 Unicode 码点；文件路径拒绝内嵌 NUL，避免误用前缀路径。
直接声明为 `c_int` 的字体指标方法要求匹配的实参类型，不会隐式收窄普通 `int`。
这不是完整字体安全沙箱：不应把此包直接用于处理不受信任用户上传的
恶意字体；该场景应另用经过专门安全审计的字体解析器。XRT 负责分配与
错误状态；C 侧的 `Font`/`Bitmap` 句柄分别用
`xstbtt_font_release`/`xstbtt_bitmap_release` 释放，
`xstbtt_default_font_path` 返回的 UTF-8 路径用 `xrtFree` 释放。
C API 见 `include/stb_truetype_native.h`，许可证见 `LICENSE.stb.txt`。

2026-10-18 访问边界复审：`Font.bitmapNative` 现仅供类内部接管字形句柄，
编译负例先红后绿。`Font/Bitmap` 的 `private extern create(point)` 仍被
当前编译器从包外直接调用；两个构造器负例只可执行 `xl check`，禁止运行。
最小复现与主线修复建议见
`demo6/dev/extlib_blockers/stb_truetype_private_handle_constructor.md`。
Windows XLang 字体/字形边界与 Linux C ASan/UBSan、XLang 双语门禁通过；
私有构造器及既有 AOT 链接问题修复前，本包不得签收发布级验收。

原生字体加载和字形栅格化入口会清除本线程上一次的 XRT 错误；
文件读取失败时保留 XRT 的原始错误类型（例如文件超限 `XERR_RANGE`、
分配失败 `XERR_MEMORY`），无原始诊断时才补 `XERR_IO`。随后成功不携带
旧错误。析构只释放资源，不覆盖调用方已有诊断。

从 `demo6` 目录进行包级验证；双语范例默认从 `xl.exe` 所在目录定位
随仓库提供的测试字体，也接受第一个命令行参数指定自己的字体路径：

- `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/stb_truetype/test/check.ps1 -SkipAot`：Windows 单入口门禁；去掉 `-SkipAot` 还会尝试完整边界 AOT。

- `.\release\xl.exe run release/examples/extlibs/stb_truetype.xl`
- `.\release\xl.exe run release/extlibs/stb_truetype/test/boundaries.xl`
- 从 `demo6/release` 目录运行 `xl run examples/extlibs/stb_truetype_c.c`；
  C 范例直接包含本包实现源，单文件运行不需要额外链接参数。
- `tools/run_extlib_c.ps1 -Package stb_truetype -Source release/examples/extlibs/stb_truetype_c.c`
- 同命令改用 `release/extlibs/stb_truetype/test/test_stb_truetype.c`
- `wsl bash release/extlibs/stb_truetype/test/check.sh` 可复跑 Linux 严格编译、
  ASan/UBSan C 边界、XLang 中央与包内用例，并逐字节比较 C/XLang 范例输出。
- C/XL 范例输出一致；测试覆盖文件/内存字体、中文 UTF-8 路径、
  输入复制、字形/空白位图、逐点分配失败、超限文件、整数回绕及路径截断边界；Windows GCC
  `-Wall -Wextra -Werror`、Linux ASan/UBSan 已通过。

带输出的完整 XL AOT 仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`；尚未发布验收。

2026-10-15：中央 `test/cases/extlib_stb_truetype.xl` 原用已删除的指针式 API，现改验对象版公开入口及无系统字体依赖的负例。C 加载/位图操作的失败后成功错误状态测试先红后绿；清单显式声明环境、路径、整文件和字符串 XRT 模块。中央无输出 AOT 构建运行通过（744,448 字节），C/XL 边界、双语逐字同输出及新增 Linux ASan/UBSan 门禁通过。带输出完整 AOT 仍缺共享快照符号。

同日追加：文件加载中的 XRT 内存失败原被覆盖为 `XERR_IO`，C 故障注入先红后绿；新增大于 32 MiB 文件的 `XERR_RANGE` 负例。Windows C/XL、中央用例、双语范例、严格 GCC 语法检查和 Linux ASan/UBSan 复跑通过。

2026-10-16：C/XLang 边界测试的中文字体夹具改用各自独立的系统临时
目录，成功后清理；包内 XLang 用例不再依赖 `release` 工作目录，
并登记到 `coverage.tests`。从 `demo6` 目录复跑 C/XLang 边界、
双语范例和 Linux ASan/UBSan 均通过；完整 AOT 阻断不变。

同日将测试和双语范例所需的系统字体替换为
`test/assets/DejaVuSans-test.ttf`：它是 DejaVu Sans 的 1,724 字节子集，
只包含空格、A、V；`test/assets/LICENSE.dejavu.txt` 保留字体许可。
源字体来自 `fonts-dejavu-core`，SHA-256 为
`B4C632E3CDF9ACC7F28758FB5A323C8524D7FC6660D46904D9B6CBE2809C419C`；
用 fontTools 4.63.0 的 `python -m fontTools.subset DejaVuSans.ttf
--unicodes=U+0020,U+0041,U+0056 --no-hinting --output-file=DejaVuSans-test.ttf`
生成。夹具未列入 package manifest，不进入 xl.exe/XRT 内置资源。
Windows C/XL 边界、双语范例与 Linux C ASan/UBSan 复跑通过；
`defaultFontPath()` 公开 API 不变，完整 AOT 阻断不变。
将范例构建到其他目录时，可把字体文件路径作为第一个命令行参数传入；
未传参时才从运行程序所在目录查找教学夹具。

2026-10-17：Linux 包级脚本补正式 `release/xl` 的中央和包内边界，
以及使用固定测试字体的 C/XLang 范例逐字节同输出检查；与 C ASan/UBSan
同次通过。此结果不代表恶意字体安全审计或完整 AOT 验收。

2026-10-20：新增 Windows 单入口门禁，独立 GCC 编译执行 C 边界，
把同一夹具路径显式传给 GCC、`xl run` 的 C 范例与 XLang 范例，逐字比较
三者输出；另运行中央/包内语言用例及 `bitmapNative` 私有访问负例。
`-SkipAot` 全部通过，独占构建目录在退出时清理。两个原生句柄构造器的
包外访问负例当前仍被 `xl check` 接受；默认门禁在边界 AOT 构建报
`xl6ValueDataSnapshotV1` 未解析，失败后也清理本次构建目录，故未签收。

同日仅裁剪测试宿主的 XRT 模块：保留字体实现与 C 用例实际使用的文件、
目录、环境、路径、字符串和分配故障模块，不再编入完整 XRT；Windows
门禁不再链接无用的网络系统库。`-SkipAot` 全部通过，单次耗时由约一分钟
降至约 11 秒；Linux C ASan/UBSan、XLang 与双语对照也通过。
