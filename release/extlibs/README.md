# DEMO6 release extlibs

本目录位于 `xl.exe` 同级目录下，存放当前可导入的扩展库候选。package/import 回归测试用 fixture 已迁移到 `test/fixtures`，测试通过 `XLANG_PACKAGE_PATH` 访问。

目录和清单仅表示提供该包，不代表完整 API 或发布候选验收。首期优先级与暂缓名单见 [扩展库生态重整](../../docs/扩展库生态重整_2026-10-12.md)。

旧 `jwt`、`oauth2` 包已移除。后续以 XRT 最新 `xjwt`、`xoauth2` 的公共 API 重新接入，不沿用旧包的实现与能力范围；替代绑定尚未发布。

## 使用方式

- `xlang.package.json` 描述包名、版本、ABI 版本、`xlSources`、`cSources`、links/loadlibs、dependencies 和 native 元数据。
- 带原生 C 源的包应在 `artifact.xrtModules` 中声明实际使用的 `XRT_MODULE_*` / `XRUNTIME_MODULE_*` 根模块，使最终产物保持精确裁剪；省略该字段表示依赖未知并保守保留完整运行时。
- 大型框架包可用 `coverage.mode = "suite"` 和 `coverage.tests` 声明包内测试；这些测试会统一完成导入与语义检查。
- `xlSources` 中的 `.xl` 文件是 import 的声明/实现来源；C 函数仍需要在 xlang 中显式 `extern` 声明。
- 包内 `.c`、`.h`、`.xl`、`bin`、`third_party` 等文件是扩展库自身实现或运行所需资产。
- 同目录同名的 `.c` 会成为 `.xl` 的自动伴随源；独立带 `main` 的 C 测试或范例须命名为 `*_c.c`。可运行 `tools/check_extlib_companions.ps1` 检查包内及 `release/examples/extlibs` 的冲突。

## 发布扩展库索引

<!-- package-index:start -->
- `cString`：显式 C 文本互操作；与语言双字段 string 分开，拒绝隐式截断内嵌 NUL。
- `floatEnv`：显式导入的浮点环境高级模块，控制当前线程的舍入模式与异常标志。
- `xmail`：新版 XRT MIME、SMTP、POP3 与 IMAP 体系的正式源码扩展包。
- `xws`：WebSocket 连接、分片 Writer、Future、TLS、压缩与连接组扩展库。
- `xhttp`：HTTP 客户端底层分片包。
- `xhttp.body`：HTTP 请求体构建分片包。
- `xhttp.body_async`：HTTP 异步请求体分片包。
- `xhttp.body_codec`：HTTP 请求体编解码分片包。
- `xhttp.body_decode`：HTTP 响应体解码分片包。
- `xhttp.body_file`：HTTP 文件请求体分片包。
- `xhttp.body_stream`：HTTP 流式请求体分片包。
- `xhttp.client`：HTTP 客户端会话分片包。
- `xhttp.form_data`：HTTP 表单数据分片包。
- `xhttp.multipart`：HTTP 多部分编码分片包。
- `xhttp.protocol`：HTTP 协议解析分片包。
- `_bzip2`：`bz2` 使用的私有 libbzip2 源码包。
- `_expat`：XML 系列扩展库使用的私有 Expat 源码包。
- `_libzip`：`zip` 使用的私有 libzip 源码包。
- `_tomlc17`：`toml` 使用的私有 tomlc17 源码包。
- `_xge_native`：内部实现包。
- `_zlib`：`gzip` 与 `zip` 共用的私有 zlib 源码包。
- `aho_corasick`：发布扩展库。
- `barcode`：EAN-13 与 UPC-A 条形码生成扩展库。
- `ansi`：终端 ANSI 转义序列构建、剥离与显示宽度扩展库。
- `argon2`：发布扩展库。
- `blake2`：发布扩展库。
- `blake3`：发布扩展库。
- `bloom`：发布扩展库。
- `brotli`：Brotli 一次性、增量与流式压缩扩展库。
- `bson`：发布扩展库。
- `bz2`：bzip2 一次性、增量与流式压缩扩展库。
- `canbus`：发布扩展库。
- `cbor`：发布扩展库。
- `chardet`：发布扩展库。
- `clickhouse`：发布扩展库。
- `cli`：命令行参数解析扩展库。
- `clip`：发布扩展库。
- `clipboard`：剪贴板扩展库。
- `coap`：发布扩展库。
- `color`：颜色解析、转换与 WCAG 对比度扩展库。
- `com`：Windows COM 扩展库。
- `consistent_hash`：发布扩展库。
- `crc32`：CRC32 扩展库。
- `crontab`：发布扩展库。
- `css`：发布扩展库。
- `csv`：流式 CSV 读写扩展库。
- `cuckoo`：发布扩展库。
- `daemon`：发布扩展库。
- `dataframe`：发布扩展库。
- `diff`：发布扩展库。
- `dinov3`：DINOv3 扩展库。
- `dns`：发布扩展库。
- `docker`：发布扩展库。
- `dotenv`：发布扩展库。
- `embeddings`：发布扩展库。
- `enet`：发布扩展库。
- `epub`：EPUB 3 电子书生成扩展库（自包含 stored-zip 容器）。
- `etcd`：发布扩展库。
- `faiss`：发布扩展库。
- `fix`：发布扩展库。
- `flatbuffers`：发布扩展库。
- `freetype`：发布扩展库。
- `ftp`：发布扩展库。
- `fuzzy`：发布扩展库。
- `geohash`：发布扩展库。
- `ggml`：发布扩展库。
- `git`：发布扩展库。
- `glad`：发布扩展库。
- `gpio`：发布扩展库。
- `graph`：图算法扩展库（拓扑排序、BFS、Dijkstra、连通分量与强连通分量）。
- `gzip`：标准 gzip 一次性、增量与流式压缩扩展库。
- `harfbuzz`：发布扩展库。
- `hcl`：发布扩展库。
- `hnswlib`：发布扩展库。
- `html5`：发布扩展库。
- `hyperloglog`：发布扩展库。
- `icu`：发布扩展库。
- `imagehash`：图像哈希扩展库。
- `gettext`：PO/MO 翻译目录解析查询扩展库。
- `ical`：RFC 5545 iCalendar 解析生成扩展库。
- `imgui`：发布扩展库。
- `influxdb`：发布扩展库。
- `ini`：保留顺序和注释的 INI 配置扩展库。
- `jieba`：发布扩展库。
- `json5`：发布扩展库。
- `json.patch`：RFC 6901 指针、RFC 6902 补丁与 RFC 7386 合并补丁扩展库。
- `kubernetes`：发布扩展库。
- `langdetect`：发布扩展库。
- `ldap`：发布扩展库。
- `leveldb`：发布扩展库。
- `libsndfile`：发布扩展库。
- `libsvm`：发布扩展库。
- `libusb`：发布扩展库。
- `linalg`：发布扩展库。
- `llama`：发布扩展库。
- `lmdb`：发布扩展库。
- `lru`：发布扩展库。
- `lz4`：LZ4 压缩扩展库。
- `lzma`：LZMA 压缩扩展库。
- `md4c`：发布扩展库。
- `md5`：发布扩展库。
- `mdns`：发布扩展库。
- `memcached`：memcached 文本协议编解码扩展库（get/set/stats，客户端走标准库 tcp）。
- `minio`：发布扩展库。
- `miniz`：miniz 压缩扩展库。
- `money`：最小货币单位整数运算与格式化扩展库。
- `mnn`：发布扩展库。
- `modbus`：MODBUS/TCP 帧编解码扩展库（MBAP over hex，客户端走标准库 tcp）。
- `mongodb`：发布扩展库。
- `mqtt`：MQTT 3.1.1 帧编解码扩展库（CONNECT/PUBLISH/SUBSCRIBE 等，客户端走标准库 tcp）。
- `msgpack`：发布扩展库。
- `murmur3`：发布扩展库。
- `mysql`：发布扩展库。
- `nanomsg`：发布扩展库。
- `ncnn`：发布扩展库。
- `neo4j`：发布扩展库。
- `netrc`：machine/default/macdef 凭据文件扩展库。
- `nsq`：发布扩展库。
- `ntp`：NTP 报文编解码扩展库（hex 帧格式，连接走标准库 udp）。
- `onnxruntime`：ONNX Runtime 扩展库。
- `op`：发布扩展库。
- `opencv`：发布扩展库。
- `opentelemetry`：发布扩展库。
- `openvino`：发布扩展库。
- `opus`：发布扩展库。
- `passkey`：发布扩展库。
- `pcap`：发布扩展库。
- `pdf`：发布扩展库。
- `pdfium`：发布扩展库。
- `pinyin`：发布扩展库。
- `pkcs`：发布扩展库。
- `pkg.math`：包导入、默认参数、导出变量与 XAPI 的内部验证包，不作为数学能力发布。
- `plist`：XML 与 binary plist 扩展库。
- `portaudio`：发布扩展库。
- `postgresql`：发布扩展库。
- `pptx`：发布扩展库。
- `procps`：发布扩展库。
- `progress`：发布扩展库。
- `prometheus`：发布扩展库。
- `qr`：二维码生成扩展库（矩阵、ASCII、半块字符与 SVG 输出）。
- `rabbitmq`：发布扩展库。
- `rapidocr`：发布扩展库。
- `ratatui`：发布扩展库。
- `redis`：RESP 协议编解码与客户端扩展库（连接走标准库 tcp）。
- `roaring`：发布扩展库。
- `rocksdb`：发布扩展库。
- `rtmp`：发布扩展库。
- `rtree`：发布扩展库。
- `rss`：RSS 2.0 与 Atom 解析生成扩展库。
- `sciter`：发布扩展库。
- `sentencepiece`：发布扩展库。
- `semver`：语义化版本解析、优先级比较与范围匹配扩展库。
- `serial`：发布扩展库。
- `sha3`：发布扩展库。
- `sharedmem`：发布扩展库。
- `singleinstance`：发布扩展库。
- `siphash128`：域分离的 128 位 SipHash 扩展库；64 位 `siphash` 已进入默认标准库。
- `sitemap`：sitemap.xml 与 robots.txt 生成解析扩展库。
- `skiplist`：发布扩展库。
- `shellwords`：POSIX/Windows 命令行参数拆分与引用扩展库。
- `snappy`：发布扩展库。
- `slugify`：URL slug 生成扩展库（Latin 转写、CJK 保留）。
- `statistics`：描述统计扩展库。
- `srt`：SRT 与 WebVTT 字幕解析生成扩展库。
- `snmp`：发布扩展库。
- `socks`：SOCKS5 握手编解码扩展库（hex 帧格式，连接走标准库 tcp）。
- `sox`：发布扩展库。
- `sqlite`：发布扩展库。
- `xssh`：XRT SSH 全协议实现及 wire、密钥、known_hosts 的语言对象。
- `stb_image`：发布扩展库。
- `stb_image_resize2`：发布扩展库。
- `stb_image_write`：发布扩展库。
- `stb_truetype`：stb_truetype 字体扩展库。
- `sysinfo`：发布扩展库。
- `tabulate`：发布扩展库。
- `taglib`：发布扩展库。
- `tar`：ustar、PAX 与 GNU TAR 归档扩展库，支持锚定根安全提取、精确白名单与谓词选择器。
- `tdengine`：发布扩展库。
- `terraform_state`：发布扩展库。
- `tokenizers`：发布扩展库。
- `toml`：TOML 解析、编码与文件读写扩展库。
- `totp`：发布扩展库。
- `treesitter`：发布扩展库。
- `trie`：发布扩展库。
- `ui.auto`：发布扩展库。
- `uia`：发布扩展库。
- `units`：字节、时长与计数 humanize 扩展库。
- `upnp`：发布扩展库。
- `vcard`：vCard 3.0/4.0 解析生成扩展库（折行与转义往返）。
- `vault`：发布扩展库。
- `vectordb`：发布扩展库。
- `volc.core`：发布扩展库。
- `volc.phone`：发布扩展库。
- `volc.sts`：发布扩展库。
- `watch`：发布扩展库。
- `webview`：发布扩展库。
- `whisper`：发布扩展库。
- `xbot`：发布扩展库。
- `xbot.agent`：发布扩展库。
- `xfeat`：XFeat 扩展库。
- `xge`：发布扩展库。
- `xge_shape_ex`：发布扩展库。
- `xml.dom`：完整 XML 文档对象模型扩展库。
- `xml.etree`：轻量 XML ElementTree 扩展库。
- `xml.sax`：流式 XML SAX 事件扩展库。
- `xregex`：完整正则表达式扩展库，提供可复用匹配器、替换、拆分与模式集合。
- `xui`：发布扩展库。
- `xxhash`：xxHash 极速哈希扩展库。
- `yaml`：发布扩展库。
- `zeromq`：发布扩展库。
- `zip`：基于 libzip 的 ZIP/ZIP64 归档扩展库。
- `zstd`：Zstandard 压缩扩展库。
- `xacme`：XRT ACME 客户端，受管 DNS／HTTP 回调与原生拥有图；离线示例见 [xacme](xacme/README.md)。
<!-- package-index:end -->

## 主要验证入口

在 `demo6` 目录运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
release\examples\extlibs\run_extlibs.bat
test\suites\10_format_extlibs.bat
```

完整 package/import 行为仍由 `test.bat` 覆盖；发布扩展库范例位于 `release/examples/extlibs`。
