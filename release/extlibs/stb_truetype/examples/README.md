# stb_truetype 双语范例

从仓库根目录运行，Windows 使用 `release/xl.exe`，Linux 使用 `./release/xl`。
使用包内带许可的 1724 字节 DejaVu 子集，只包含空格、A、V；不依赖
本机系统字体，也不通过下载获得测试输入。

```powershell
release/xl.exe run release/extlibs/stb_truetype/examples/mask.xl
release/xl.exe run release/extlibs/stb_truetype/examples/mask_c.c
release/xl.exe build release/extlibs/stb_truetype/examples/mask.xl -o build/glyph-mask.exe
./build/glyph-mask.exe
```

也可在 `--` 后传入字体路径；独立程序将路径作为第一个实参。
默认工作目录是仓库根目录，自选字体不会保证相同像素或输出尺寸。
固定测试字体的完整预期在 `mask.expected.txt`：

```text
glyph A mask
..+#+..
..+#+..
..#+#+.
.+#.#+.
.#+.+#.
+#####+
+#+++#+
#+...+#
independent snapshot: true
```

`#` 表示 alpha ≥128，`+` 表示非零但不足 128，`.` 表示透明。字形
高度参数为 12；这里的字号是字体垂直度量比例，不保证黑色笔画正好
占 12 行。C 和 XLang 采用同一阈值、逐行顺序及相同输出。

XLang 先由 `loadBytes` 复制输入，清空原文件 Buffer 后继续渲染，返回
宽、高和 `glyph.pixels()`。调用者得到结果时 Font/Bitmap 均已退役。
C 对照先释放输入，然后渲染并复制到独立 XRT Buffer，释放字体与位图
后再显示掩码。两者均没有临时输出文件，不额外生成图片或持久缓存。

这是明确拥有型副本：字体输入复制一次，位图转独立 Buffer 再复制一次。
C 若只在位图存活期使用像素，可直接借用 `xstbtt_bitmap_data`，不要释放
它或跨位图退役使用；不能拿借用方式与本例的独立寿命承诺混为零成本。

独立 GCC（Windows）：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt release/extlibs/stb_truetype/examples/mask_c.c release/extlibs/stb_truetype/stb_truetype_native.c release/extlibs/stb_truetype/test/xrt_runtime.c -lm -o build/glyph-mask-c.exe
./build/glyph-mask-c.exe
```

Linux 使用同三个源文件，加 `-D_GNU_SOURCE -pthread -ldl`。包级门禁逐字
对照 GCC、C 内存运行、C AOT、XLang 内存运行与 AOT；原指标范例仍在
`release/examples/extlibs/stb_truetype.xl` 和同名 `_c.c`，完整类表面在
`test/api_surface.xl`。后者允许系统字体未安装，不跳过随包字体测试。

`defaultFontPath()` 只是一项便利查询：空串表示未找到，真实错误不继续
尝试别的候选。新严格探测对缺失项有诊断构造成本，不适合逐帧查找。
生产程序应使用随附的可信字体。此包不是处理恶意字体的安全沙箱，
主线仍未拒绝包外私有句柄构造器；两个只编译负例从不运行。
