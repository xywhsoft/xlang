# 共享主线阻断：文本/值链路的 AOT 链接缺少 xl6ValueDataSnapshotV1

- 验证时间：2026-10-12；当前 `demo6/release/xl.exe`（2026-10-07 构建）和同目录 DLL。
- 最小复现：在 `demo6/release` 执行 `xl run ../dev/extlib_blockers/text_snapshot_link.xl` 成功并输出 `hello`；执行 `xl build ../dev/extlib_blockers/text_snapshot_link.xl -o ../build/test/text_snapshot_probe.exe` 失败。
- 期望：生成可执行文件并输出 `hello`。
- 实际：TCC 报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。无 `println` 的 `examples/hello.xl` 可 AOT 构建。
- 影响面：`ini` 和 `blake2` 的 XL 范例 `xl run` 均通过，但 AOT 构建同样失败。不是包源码或哈希算法自身的缺陷；任何触发同一文本输出 runtime 链路的包都可能受影响。
- 可核查的主线线索：`src/runtime/xl6_value_text.c` 调用 `xl6ValueDataSnapshotV1`，实现位于 `src/runtime/xl6_array_graph.c`；`src/compile/xl6_compile_unit.c` 已声明 VALUE_TEXT 依赖 ARRAY_GRAPH。应核查 AOT 依赖闭包、实际编入单元和符号导出/裁剪条件，保证定义进入最终 TCC 链接。修复后用本文件跑 JIT/AOT，再复跑受影响包的发布门禁。
- 状态：blocked-upstream；本扩展库任务不修改 `src/` 主线。

2026-10-17 当前 `release/xl.exe` 复核：在 `demo6` 执行
`release/xl.exe build dev/extlib_blockers/text_snapshot_link.xl -o build/test/text_snapshot_probe.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`，退出码 1。
同次独立复核的动态索引和原生句柄探针也分别缺各自符号；此条只证明
当前发布构建仍阻断完整 AOT，不推断尚在开发的主线源码状态。

2026-10-14 `json.patch` 最小包级复现：在 `demo6/release` 运行
`.\xl.exe run extlibs/json.patch/test/aot_core.xl` 返回 0；同文件执行
`.\xl.exe build extlibs/json.patch/test/aot_core.xl -o ../build/test/json_patch_aot_core.exe`
报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。用例只创建一个对象并
调用 `has`/`get`，没有输出语句；期望构建并返回 0。由此不能把缺符号归因于
显式 `println`，也不能以包侧模块声明掩盖共享 AOT 依赖闭包问题。

2026-10-14 `redis` 更窄的布尔探针：在 `demo6/release` 运行
`./xl.exe run extlibs/redis/test/aot_core.xl` 返回 0；运行
`./xl.exe build extlibs/redis/test/aot_core.xl -o ../build/test/redis_aot_core.exe`
报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。用例只调用
`redis.isComplete` 判断一条完整与一条截断 RESP 回复，没有输出或容器索引；
期望生成 exe 并返回 0。包内未以未经验证的 XRT 模块清单绕过此链接缺口。

2026-10-13 复核：从 `demo6/release` 执行
`./xl.exe run extlibs/rapidocr/test/real_inference.xl` 通过真实模型识别；执行
`./xl.exe build extlibs/rapidocr/test/real_inference.xl -o ../build/test/rapidocr/real_inference_aot.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。RapidOCR 的 Windows/Linux C 门禁、
XLang JIT 和真实模型快照测试均通过；修复共享符号后需重跑完整 AOT 门禁。

`rss` 的无输出 AOT 核心探针 `release/extlibs/rss/test/aot_core.xl` 可构建并运行；
在 `demo6` 下执行 `release/xl.exe build release/examples/extlibs/rss.xl -o build/test/rss_example.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。XLang/C 范例的 JIT 路径已通过，
完整 RSS 边界 AOT 另受 `value_index_link.md` 记录的索引符号缺失影响。

`trie` 的完整无输出边界 `test/cases/extlib_trie.xl` 可 AOT 构建并运行；
在 `demo6` 下执行 `release/xl.exe build release/examples/extlibs/trie.xl -o build/test/trie_example.exe`
仍报同一 `xl6ValueDataSnapshotV1` 缺失符号。包级 C/JIT、Linux ASan/UBSan 和双语范例已通过。

`skiplist` 的无动态结果核心探针 `release/extlibs/skiplist/test/aot_core.xl` 可 AOT 构建运行；
在 `demo6` 下执行 `release/xl.exe build test/cases/extlib_skiplist.xl -o build/test/skiplist_guard.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`，带输出范例亦同。
包内 C/JIT、Linux ASan/UBSan、双语结果已验证。应修复共享 AOT 依赖闭包后重跑完整用例。

2026-10-12 补充：`dotenv` 独立包级门禁的 C 测试、C/XL 范例、XL 边界 JIT、
XL 边界 AOT 均通过；仅 `release/extlibs/dotenv/test/check.ps1` 的
`XLang example build` 报同一缺失符号。因此问题与 `dotenv` 解析实现无关。

`color` 的专项门禁中独立 GCC、线程、外部 oracle、XLang 测试 JIT/AOT
也通过；仅带输出的 `color.xl` 范例 AOT 在相同符号处失败。

`netrc` 的 `release/extlibs/netrc/test/check.ps1 -SkipAot` 通过严格 GCC、
C/XL 测试和双语范例；去掉 `-SkipAot` 时，构建不含 `println` 的
`release/extlibs/netrc/test/boundaries.xl` 仍在相同符号处失败。
说明该 AOT 缺口不只由显式文本输出触发，还会影响包解析/序列化链路。

`shellwords` 的 C/XL 边界与双语范例 JIT 通过，
`xl build examples/extlibs/shellwords.xl -o ../build/test/shellwords_guard/shellwords_example_probe.exe`
仍在同一符号处失败。索引读取另有独立 AOT 缺符号，见
`value_index_link.md`。

`semver` 的 `release/extlibs/semver/test/check.ps1 -SkipAot` 通过严格 GCC、
C/XL 边界与双语黄金输出；完整脚本中的无输出边界用例也能 AOT 编译运行。
但 `xl build extlibs/semver/examples/basic.xl -o ../build/test/semver_guard/semver_xl_example.exe`
报同一缺失符号。该问题不影响 SemVer 解析本体，修复共享 AOT 链接后重跑完整脚本。

`sysinfo` 的 Windows/Linux C 测试、Windows XL JIT、双语对照和无输出边界 AOT
均通过；`xl build extlibs/sysinfo/examples/basic.xl -o ../build/test/sysinfo_guard/sysinfo_xl_example.exe`
仍报同一缺失符号。修复共享文本 AOT 链路后重跑 `sysinfo/test/check.ps1`。

`gettext` 的 `release/extlibs/gettext/test/check.ps1 -SkipAot` 通过严格 GCC、
PO/MO 边界、C/XL 双语输出对照；完整门禁的 `boundaries.xl` AOT 也通过。
仅 `xl build extlibs/gettext/examples/gettext.xl -o ../build/test/gettext_guard/gettext_xl_example.exe`
因相同的 `xl6ValueDataSnapshotV1` 缺失而失败。Linux 严格 GCC 与
ASan/UBSan 包级测试通过。共享符号修复后重跑完整门禁。

`cString` 仅把 CRT `strlen` 显式导入 XLang；Win/Linux C 测试、
XL JIT、无输出 `boundaries.xl` AOT 和双语范例输出对照均通过。
`xl build extlibs/cString/examples/cString.xl -o ../build/test/cString_guard/cString_xl_example.exe`
在同一 `xl6ValueDataSnapshotV1` 符号处失败；包自身无 C 源和链接依赖。

`md4c` 的 `release/extlibs/md4c/test/check.ps1 -SkipAot` 通过严格 GCC、
结果所有权与错误边界、双语范例输出对照；无输出 `boundaries.xl` AOT 通过。
`xl build extlibs/md4c/examples/md4c.xl -o ../build/test/md4c_guard/md4c_xl_example.exe`
仍在同一 `xl6ValueDataSnapshotV1` 符号处失败。Linux 严格 GCC、
ASan/UBSan 包级测试已通过。

`watch` 原先的 `xwatch_exists` C `long long`/XL `bool` ABI 不一致由包内修正，
现有 Win/Linux C 与 XLang JIT、UTF-8 文件/目录事件、Linux ASan/UBSan、
双语范例对照和无输出边界 AOT 均通过。
`xl build extlibs/watch/examples/watch.xl -o ../build/test/watch_guard/watch_xl_example.exe`
仍因相同的 `xl6ValueDataSnapshotV1` 缺失而失败。

`cbor` 的无输出核心 AOT 编解码探针通过；
`xl build extlibs/cbor/examples/cbor.xl -o ../build/test/cbor_guard/cbor_xl_example.exe`
仍在带输出的范例处缺少 `xl6ValueDataSnapshotV1`。其另一个容器索引
AOT 阻断见 `value_index_link.md`。

`bson` 的 C/XL 范例 JIT 输出完全相同；
`demo6/release` 下执行
`xl build extlibs/bson/examples/bson.xl -o ../build/test/bson_guard/bson_xl_example.exe`
报相同的 `unresolved reference to 'xl6ValueDataSnapshotV1'`。无输出核心
AOT 已通过；与容器索引 AOT 缺符号共同阻止发布验收。

`msgpack` 的 `release/extlibs/msgpack/test/check.ps1 -SkipAot` 通过严格 GCC、
Windows/Linux C 边界、Linux ASan/UBSan、XL JIT、C/XL 范例逐字对照，
无输出的核心与边界测试 AOT 也通过。`demo6/release` 下执行
`xl build extlibs/msgpack/examples/msgpack.xl -o ../build/test/msgpack_xl_example.exe`
仍缺少 `xl6ValueDataSnapshotV1`；修复共享 AOT 文本链接后重跑完整包级脚本。

`json5` 的 C/XL 边界与范例、Linux ASan/UBSan、无输出核心 AOT 已通过；
`demo6/release` 下执行
`xl build extlibs/json5/examples/json5.xl -o ../build/test/json5_guard/json5_xl_example.exe`
仍缺少 `xl6ValueDataSnapshotV1`。修复共享 AOT 链接后复跑包级脚本。
此包另有独立的 Unicode 未加引号键名规范缺口，不应因 AOT 修复而直接签收。

`toml` 的 C 测试、Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 日期
往返和 C/XL 公开示例均通过；`demo6/release` 下执行
`xl build extlibs/toml/test/boundaries.xl -o ../build/test/json5_guard/toml_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。包内曾有普通对象
日期探测遗留 XRT 错误的问题，已独立修复；这次 AOT 缺符号需要主线线程处理。

`xml.dom` 的 C/XL DOM、DTD 禁用测试、Windows 严格 GCC、Linux ASan/UBSan
和双语示例通过；`demo6/release` 下执行
`xl build extlibs/xml.dom/test/boundaries.xl -o ../build/test/json5_guard/xml_dom_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。本任务不改主线；
修复共享运行时依赖闭包后复跑 `xml.dom` 包级门禁。

`xml.etree` 的 C/XL 范例输出一致，XL JIT 边界、Windows 严格 GCC、
Linux ASan/UBSan 测试通过；`demo6/release` 下执行
`xl build extlibs/xml.etree/test/boundaries.xl -o ../build/test/json5_guard/xml_etree_xl_boundaries.exe`
同样报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。修复主线链接
依赖闭包后需重跑 AOT 和包级门禁。

`xml.sax` 的 C/XL SAX、回调、DTD 禁用、错误位置、Windows 严格 GCC、
Linux ASan/UBSan 和双语示例均通过；`demo6/release` 下执行
`xl build extlibs/xml.sax/test/boundaries.xl -o ../build/test/json5_guard/xml_sax_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复后
重跑 SAX AOT 包级门禁。

`zip` 的内存归档 C/XL 例程、参数与读取边界、C 包级套件和原生桥接
Windows/Linux GCC 严格语法检查通过；`demo6/release` 下执行
`xl build extlibs/zip/test/boundaries.xl -o ../build/test/json5_guard/zip_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复后
重跑 ZIP AOT 与归档包级门禁。

`hcl` 已改用独立结果所有权，Windows/Linux 严格 GCC、Linux ASan/UBSan、
四线程查询、长值及 C/XL 双语范例通过；`demo6/release` 下执行
`xl build extlibs/hcl/test/boundaries.xl -o ../build/test/json5_guard/hcl_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需由主线线程修复
运行时依赖闭包后复跑 HCL AOT 包级门禁。

`css` 的 C/XL 边界、Windows 严格 GCC、Linux ASan/UBSan、四线程查询与
双语范例均通过；`demo6/release` 下执行
`xl build extlibs/css/test/boundaries.xl -o ../build/test/json5_guard/css_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需由主线线程
修复运行时依赖闭包后复跑 CSS AOT 包级门禁。

`redis` 的 C/XL RESP2 边界、Windows 严格 GCC、Linux ASan/UBSan、
四线程以及本机随机端口命令回环测试通过；`demo6/release` 下执行
`xl build extlibs/redis/test/boundaries.xl -o ../build/test/json5_guard/redis_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需由主线线程
修复依赖闭包后重跑 Redis AOT；真实 Redis 服务成功路径另需验收。

`stb_image` 的 C/XL 文件与内存解码、UTF-8 中文路径、Windows 严格 GCC、
Linux ASan/UBSan、四线程和双语范例通过；`demo6/release` 下执行
`xl build extlibs/stb_image/test/boundaries.xl -o ../build/test/json5_guard/stb_image_boundaries_xl.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需由主线线程修复
依赖闭包后重跑图片解码 AOT 门禁。

`stb_image_write` 的 C/XL 五格式编码、UTF-8 中文路径、Windows 严格 GCC、
Linux ASan/UBSan 和双语范例通过；`demo6/release` 下执行
`xl build extlibs/stb_image_write/test/boundaries.xl -o ../build/test/json5_guard/stb_image_write_boundaries_xl.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享链接
依赖闭包后重跑图片编码 AOT 门禁。

`stb_image_resize2` 的 C/XL sRGB/linear 缩放、步长、短缓冲边界、
Windows 严格 GCC、Linux ASan/UBSan 和双语范例通过；`demo6/release` 下执行
`xl build extlibs/stb_image_resize2/test/boundaries.xl -o ../build/test/json5_guard/stb_image_resize2_boundaries_xl.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享链接
依赖闭包后重跑图片缩放 AOT 门禁。

`stb_truetype` 的 C/XL 文件与内存字体、UTF-8 中文路径、自动析构、
Windows 严格 GCC、Linux ASan/UBSan 和双语范例通过；`demo6/release` 下执行
`xl build extlibs/stb_truetype/test/boundaries.xl -o ../build/test/json5_guard/stb_truetype_boundaries_xl.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享链接
依赖闭包后重跑字体包 AOT 门禁；包的可信字体输入范围见 README。

`linalg` 的 C/XL 尺度边界、输出参数事务语义、Windows 严格 GCC、
Linux ASan/UBSan 和双语范例通过；`demo6/release` 下执行
`xl build extlibs/linalg/test/boundaries.xl -o ../build/test/json5_guard/linalg_boundaries_xl.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线线程应复核 AOT
依赖闭包和当前发布二进制是否滞后，修复后重跑线性代数包门禁。

`sharedmem` 的 Windows 独立 GCC 跨进程、Linux GCC + ASan/UBSan 子进程、
XLang JIT 边界与 C/XL 对照范例均通过。在 `demo6/release` 执行
`xl build extlibs/sharedmem/test/boundaries.xl -o ../build/test/sharedmem_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。重现不要求修改
任何主线源码；修复 AOT 依赖闭包后重跑此命令及包内 README 的定向门禁。

`singleinstance` 的 C/XL 包级测试、Windows/Linux 独立进程互斥、异常退出
回收、Linux ASan/UBSan 和双语范例通过。在 `demo6/release` 执行
`xl build extlibs/singleinstance/test/boundaries.xl -o ../build/test/singleinstance_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。修复共享 AOT
链接闭包后重跑本命令，不应在扩展库层增加重复 runtime 实现。

`procps` 的 Windows/Linux C 边界、Windows XL JIT、Linux ASan/UBSan 与
C/XL 范例对照通过。在 `demo6/release` 执行
`xl build extlibs/procps/test/boundaries.xl -o ../build/test/procps_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。这与进程枚举
的 Windows/Linux 实现无关；主线修复共享 AOT 依赖闭包后复跑包级门禁。

`pkcs` 的常用 DER 构件、独立结果所有权、非法输入、零整数、int64 边界、
Windows 严格 GCC、Linux ASan/UBSan、XL JIT 和 C/XL 对照范例通过。
在 `demo6/release` 执行
`xl build extlibs/pkcs/test/boundaries.xl -o ../build/test/pkcs_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。这是共享 AOT
链接依赖闭包问题；主线修复后重跑此命令，包内不增加重复 runtime 实现。

`ansi` 的独立结果所有权、长文本、错误边界、CSI/OSC、Windows 严格 GCC、
Linux ASan/UBSan、XL JIT 和 C/XL 对照范例通过。在 `demo6/release` 执行
`xl build extlibs/ansi/test/boundaries.xl -o ../build/test/ansi_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享 AOT
链接依赖闭包后重跑本命令及包级测试；不在包内复制 runtime 符号。

`consistent_hash` 的 XRT 哈希环、容量/错误边界、Windows/Linux 严格 GCC、
Linux ASan/UBSan、C/XL 对照及 Windows/Linux 并发读写测试通过。
在 `demo6/release` 执行
`xl build extlibs/consistent_hash/test/boundaries.xl -o ../build/test/consistent_hash_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享 AOT
依赖闭包后重跑；包侧不制造第二份快照实现。

`lru` 的 XRT 映射、LRU/LFU 淘汰、长值与错误边界、Windows 严格 GCC、
Linux ASan/UBSan、C/XL 对照和同实例并发读写通过。在 `demo6/release` 执行
`xl build extlibs/lru/test/boundaries.xl -o ../build/test/lru_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复链接
依赖闭包后重跑，不在扩展包中引入重复 runtime。

`diff` 的有界 LCS、长行/长输出、CRLF、UTF-8、结果所有权与线程安全，
Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和 C/XL 对照通过。
在 `demo6/release` 执行
`xl build extlibs/diff/test/boundaries.xl -o ../build/test/diff_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享
AOT 链接闭包后复跑此命令；`test/cases/extlib_diff.xl` 的无输出核心
AOT 已构建并运行成功，包内不复制 runtime 实现。

`floatEnv` 的舍入模式、异常标志、嵌套快照、hold 恢复和跨线程拒绝测试，
Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 与 C/XL 同输出范例通过。
在 `demo6/release` 执行
`xl build extlibs/floatEnv/test/boundaries.xl -o ../build/test/float_env_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线线程修复
共享 AOT 链接依赖闭包，然后复跑此命令；`test/cases/stdlib_crt_float_env.xl`
的无输出核心 AOT 已构建并运行成功，包内不增加第二份 runtime。

`fuzzy` 的 XRT Unicode 编辑距离、UTF-8/长度边界、长候选独立结果与
并发调用，Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和 C/XL
双语对照通过；`test/cases/extlib_fuzzy.xl` 无输出核心 AOT 构建运行成功。
在 `demo6/release` 执行
`xl build extlibs/fuzzy/test/boundaries.xl -o ../build/test/fuzzy_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享文本
快照链接闭包后复跑完整边界程序，包内不复制 runtime 实现。

`geohash` 的 C/XL ABI、严格编码、经线环绕与极点邻格、独立结果及
并发测试通过 Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和双语
范例；`test/cases/extlib_geohash.xl` 无输出核心 AOT 已构建并运行成功。
在 `demo6/release` 执行
`xl build extlibs/geohash/test/boundaries.xl -o ../build/test/geohash_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线应核查
`VALUE_TEXT` 到 `ARRAY_GRAPH` 的 AOT 链接依赖闭包，修复后重跑完整边界
程序；包内不复制 runtime 实现。

`hyperloglog` 的 C/XL ABI、rank 与 XRT 哈希、二进制长度、10 万/2 万项
统计余量、独立实例线程测试通过 Windows/Linux 严格 GCC、Linux
ASan/UBSan、XL JIT 和双语范例；`test/cases/extlib_hyperloglog.xl`
无输出核心 AOT 已构建并运行成功。在 `demo6/release` 执行
`xl build extlibs/hyperloglog/test/boundaries.xl -o ../build/test/hyperloglog_xl_test.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享
文本快照链接依赖闭包后复跑完整边界程序；包内不复制 runtime 实现。

`totp` 的 RFC 4226/6238 向量、严格 Base32、长密钥、窗口、独立结果与
并发测试通过 Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和双语
范例；`test/cases/extlib_totp.xl` 无输出核心 AOT 已构建并运行成功。
在 `demo6/release` 执行
`xl build extlibs/totp/test/boundaries.xl -o ../build/test/totp_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享
文本快照链接依赖闭包后复跑完整边界程序；包内不复制 runtime 实现。

`units` 的 IEC/SI 单位、错误与负数分离、独立结果及并发测试通过
Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和双语范例；
`test/cases/extlib_units.xl` 无输出核心 AOT 已构建并运行成功。
在 `demo6/release` 执行
`xl build extlibs/units/test/boundaries.xl -o ../build/test/units_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享
文本快照链接依赖闭包后复跑完整边界程序；包内不复制 runtime 实现。

`slugify` 的 C/XL ABI、严格 UTF-8、动态结果、长标题和并发测试通过
Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和双语范例；
`test/cases/extlib_slugify.xl` 无输出核心 AOT 已构建并运行成功。
在 `demo6/release` 执行
`xl build extlibs/slugify/test/boundaries.xl -o ../build/test/slugify_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享
文本快照链接依赖闭包后复跑完整边界程序；包内不复制 runtime 实现。

`tabulate` 的数组单元格、事务回滚、动态结果、中文宽度、1 MiB 预算与
并发测试通过 Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和双语
范例；`test/cases/extlib_tabulate.xl` 无输出核心 AOT 已构建并运行成功。
在 `demo6/release` 执行
`xl build extlibs/tabulate/test/boundaries.xl -o ../build/test/tabulate_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。需主线修复共享
文本快照链接依赖闭包后复跑完整边界程序；包内不复制 runtime 实现。

`srt` 的 C/XL 字幕解析生成及双语同输出范例通过，
`xl build extlibs/srt/test/core_aot.xl -o ../build/test/srt_core.exe`
构建并运行成功。在 `demo6/release` 执行
`xl build examples/extlibs/srt_pair.xl -o ../build/test/srt_pair.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。输出路径需要主线
补齐共享 AOT 文本快照依赖；包内不复制 runtime 实现。

`sitemap` 的 C/XL 同输出范例及核心 AOT 已验证。在 `demo6/release` 执行
`xl build extlibs/sitemap/test/core_aot.xl -o ../build/test/sitemap_core.exe`
构建并运行成功；`xl build examples/extlibs/sitemap_pair.xl -o ../build/test/sitemap_pair.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。包内不复制 runtime
实现；主线补齐共享 AOT 文本快照依赖后重跑双语范例构建。

`progress` 的包内无输出边界 `extlibs/progress/test/boundaries.xl` 已构建并
运行成功；C/XLang 打印范例输出一致。在 `demo6/release` 执行
`xl build examples/extlibs/progress.xl -o ../build/test/progress_example.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线补齐共享文本
快照 AOT 依赖闭包后重跑范例；包内不复制 runtime 实现。

`pinyin` 的 C/XLang 边界和双语打印范例已通过，
`xl build extlibs/pinyin/test/boundaries.xl -o ../build/test/pinyin_boundaries.exe`
构建并运行成功。在 `demo6/release` 执行
`xl build examples/extlibs/pinyin.xl -o ../build/test/pinyin_example.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线补齐共享文本
快照 AOT 依赖闭包后重跑带输出范例；包内不复制 runtime 实现。

`qr` 的 C/XLang 边界、独立 ZXing 扫码和无输出 AOT 均通过；在
`demo6/release` 执行
`xl build extlibs/qr/test/boundaries.xl -o ../build/test/qr_boundaries.exe`
构建并运行成功，而
`xl build examples/extlibs/qr.xl -o ../build/test/qr_example.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线补齐共享文本
快照 AOT 依赖闭包后重跑双语范例；包内不复制 runtime 实现。

`rtree` 的 2048 项 C 暴力对照、故障注入及 600 项 XLang 边界通过；
`xl build extlibs/rtree/test/boundaries.xl -o ../build/test/rtree_boundaries.exe`
构建并运行成功。在 `demo6/release` 执行
`xl build examples/extlibs/rtree.xl -o ../build/test/rtree_example.exe`
仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线补齐共享文本
快照 AOT 依赖闭包后重跑带输出范例；包内不复制 runtime 实现。

`terraform_state` 的严格 GCC、C 边界、XL JIT、C/XL 同输出范例和无输出
边界 AOT 均通过。在 `demo6/release` 执行
`xl build extlibs/terraform_state/examples/basic.xl -o ../build/test/terraform_state_basic.exe`
报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。主线修复共享文本
快照依赖后，去掉包内 `test/check.ps1` 的 `-SkipAot` 重跑完整验收。

`serial` 在当前发布物上的严格 GCC、C 回环及故障注入、生成原型、C/XL JIT
通过。在 `demo6` 执行 `pwsh -NoProfile -File tools/check_extlib_serial.ps1`，
首次 `xlang-build` 报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。
主线修复后重跑完整门禁；真实设备和 Linux 硬件路径另按包内 README 验收。

`clipboard` 在 `demo6` 执行
`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/clipboard/test/check.ps1 -SkipAot`
通过严格 GCC、隔离 window station 的 C/XL JIT 写入、双语范例和生成 C ABI；
不带 `-SkipAot` 时，首次 `XLang AOT build` 报相同缺失符号。
修复主线 AOT 依赖闭包后复跑完整包级门禁；剪贴板的并发与快照部分失败
仍是独立的包内未完成项。
2026-10-21 复验：隔离公开综合范例已加入 `check.ps1`，`-SkipAot` 全通过；
默认门禁仍在首次 `XLang AOT build` 报
`unresolved reference to 'xl6ValueDataSnapshotV1'`，不是新增范例的失败。

`onnxruntime` 的真实模型推理测试 `extlibs/onnxruntime/test/model_probe.xl`
在 JIT/AOT 均通过；`demo6/release` 下执行
`xl build extlibs/onnxruntime/examples/model_info.xl -o ../build/test/onnxruntime_info.exe`
在带 `println` 的范例中仍报 `unresolved reference to 'xl6ValueDataSnapshotV1'`。
同名 C 范例与 XL 自动混编造成的双 `main` 已通过把 C 范例命名为
`model_info_c.c` 在包内消除，不属于主线缺陷。
