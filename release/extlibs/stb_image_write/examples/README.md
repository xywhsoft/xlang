# BMP 内存编码对照

从仓库根目录运行，无外部素材，不写文件：

```powershell
./release/xl.exe run release/extlibs/stb_image_write/examples/bmp_layout.xl
./release/xl.exe run release/extlibs/stb_image_write/examples/bmp_layout_c.c
./release/xl.exe build release/extlibs/stb_image_write/examples/bmp_layout.xl -o bmp_layout.exe
./bmp_layout.exe
```

Linux 将 `./release/xl.exe` 换为 `./release/xl`，独立程序名不加 `.exe`。
C 源同样可直接 `build`；包门禁另外使用 GCC 构建并运行 C 版本。两者
自行断言，输出完全一致：

```text
gray BMP: 58 bytes, pixels at 54
RGB BMP: 70 bytes, pixels at 54
RGBA BMP: 126 bytes, pixels at 122
source unchanged: true
stb write BMP layout: OK
```

示例分别编码 1×1 灰度、2×2 RGB、1×1 RGBA；前两者使用 54 字节头，
RGB 行补齐至四字节，RGBA 使用 122 字节 V4 头并保留 alpha。返回的
Buffer 独立拥有；C 用 `xrtBufferDestroy`，XLang 自动释放。输入仅借用。
双通道输入的 alpha 不在 24 位 BMP 中保存，不应当作 RGBA 替代。

原五格式文件例仍在 `release/examples/extlibs/stb_image_write.xl` 和
`stb_image_write_c.c`：可传输出目录和素材路径，验证实际编码文件。
64 MiB 输出预算及分配失败专项放在 `test`，不塞入日常示例。

# HDR 值域范例

`hdr_values.xl` 与 `hdr_values_c.c` 在内存中编码0、0.125、0.5、1、2、4、8、16，
默认输出相同四行，不生成文件。可从仓库根运行：

```powershell
release/xl.exe run release/extlibs/stb_image_write/examples/hdr_values.xl
release/xl.exe run release/extlibs/stb_image_write/examples/hdr_values_c.c
```

传 `-- output.hdr` 可保留编码文件；C与XLang均走本包公开原子写出接口。
XLang固定字节夹具按当前小端x64 float32编写，C使用本机float数组。
真正的HDR像素通常由`stb_image.loadf(...).pixels()`取得；alpha不进入RGBE，
负/非有限颜色或超出RGBE指数范围会报告错误，而不是静默写坏图片。
