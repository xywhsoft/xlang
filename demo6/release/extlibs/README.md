# DEMO6 release extlibs

本目录位于 `xl.exe` 同级目录下，只存放可发布的功能扩展库。package/import 回归测试用 fixture 已迁移到 `demo6/test/fixtures`，测试通过 `XLANG_PACKAGE_PATH` 访问。

## 使用方式

- `xlang.package.json` 描述包名、版本、ABI 版本、`xlSources`、`cSources`、links/loadlibs、dependencies 和 native 元数据。
- `xlSources` 中的 `.xl` 文件是 import 的声明/实现来源；C 函数仍需要在 xlang 中显式 `extern` 声明。
- 包内 `.c`、`.h`、`.xl`、`bin`、`third_party` 等文件是扩展库自身实现或运行所需资产。

## 发布扩展库索引

<!-- package-index:start -->
- `clipboard`：剪贴板扩展库。
- `com`：Windows COM 扩展库。
- `crc32`：CRC32 扩展库。
- `dinov3`：DINOv3 扩展库。
- `imagehash`：图像哈希扩展库。
- `lz4`：LZ4 压缩扩展库。
- `lzma`：LZMA 压缩扩展库。
- `mail.imap`：IMAP 邮件扩展库。
- `mail.mime`：MIME 邮件扩展库。
- `mail.pop3`：POP3 邮件扩展库。
- `mail.smtp`：SMTP 邮件扩展库。
- `md4c`：Markdown/HTML 扩展库。
- `md5`：MD5 扩展库。
- `miniz`：miniz 压缩扩展库。
- `onnxruntime`：ONNX Runtime 扩展库。
- `opencv`：OpenCV 扩展库。
- `pkg.math`：数学扩展库。
- `rapidocr`：RapidOCR 扩展库。
- `redis`：Redis 协议扩展库。
- `sqlite`：SQLite 扩展库。
- `stb_image`：stb_image 图片读取扩展库。
- `stb_image_resize2`：stb_image_resize2 图片缩放扩展库。
- `stb_image_write`：stb_image_write 图片写入扩展库。
- `stb_truetype`：stb_truetype 字体扩展库。
- `ui.auto`：UI 自动化扩展库。
- `uia`：Windows UI Automation 扩展库。
- `volc.core`：火山引擎核心工具扩展库。
- `volc.phone`：火山引擎电话服务扩展库。
- `volc.sts`：火山引擎 STS 扩展库。
- `xfeat`：XFeat 扩展库。
- `zstd`：Zstandard 压缩扩展库。
<!-- package-index:end -->

## 主要验证入口

在 `demo6` 目录运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
release\examples\extlibs\run_extlibs.bat
```

完整 package/import 行为仍由 `test.bat` 覆盖；发布扩展库范例位于 `release/examples/extlibs`。
