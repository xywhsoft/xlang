# srt

`srt` 解析、生成 SRT 和常用 WebVTT 字幕。解析后返回 cue 数组，每项包含 `index`、`startMs`、`endMs` 和字符串数组 `lines`。WebVTT 中还可带 `id`、`settings`。时间单位都是毫秒；`build` 会把 SRT 序号按数组顺序重新编号。

## XLang API

- `srt.parse(text)` / `srt.build(cues)`：SRT。需要正整数序号和 `HH:MM:SS,mmm` 时间戳。
- `srt.parseVtt(text)` / `srt.buildVtt(cues)`：WebVTT。需要 `WEBVTT` 头及空行，支持 cue ID、普通设置串、`MM:SS.mmm` 与 `HH:MM:SS.mmm`。生成时统一输出小时格式。
- `srt.parseTime(stamp)` / `srt.formatTime(milliseconds)`：解析 SRT/VTT 时间戳；格式化为 SRT 格式。非法值抛 `ValueError`。

解析接受 LF/CRLF 和 UTF-8 BOM，拒绝孤立 `\r`；正文按原样保留 UTF-8 字节和行内空白，拒绝内嵌 NUL 以免静默截断。WebVTT `NOTE`（含空格或 tab 注释标题）、`STYLE`、`REGION` 块跳过，**不保留**样式内容，也不解释 cue 文本标记/定位设置的语义。这是字幕文本与时间轴库，不是完整 WebVTT 浏览器排版引擎。生成时 cue 必须具有非负、顺序正确的整数时间和字符串行；行内换行、NUL、空行不被接受，以免输出不可往返的文档。WebVTT 的 `id` 不得与控制块标题冲突，解析与生成均拒绝 `settings` 中的第二个 `-->`；生成时 `id` 也不得包含 `-->`。时间戳和 SRT 序号按 `int64` 范围检查，不提前收窄有效上界。无固定行长或 64 KiB 输出上限。

SRT/WebVTT 输入整体必须是有效 UTF-8；C 值模型构建时，cue 正文、WebVTT `id` 和 `settings` 也必须是有效 UTF-8。XRT 字符串本身可保存任意字节，因此这些检查在字幕包边界执行。

## C API 与所有权

公开声明位于 `include/srt_native.h`，依赖 XRT 的 `xvalue`。C 字符串可调用 `xsrt_parse` / `xsrt_parse_vtt`，显式字节长度输入使用 `_n` 后缀入口；XLang 桥只用 `_n` 入口，避免内嵌 NUL 被截断。解析结果由调用方 `xrtValueRelease`；格式错误返回 `XVALUE_NULL`，同时设置 XRT 错误。`xsrt_build` / `xsrt_build_vtt` / `xsrt_format_time` 返回独立 NUL 结尾缓冲，调用方 `xrtFree`；失败返回 `NULL`。无静态返回缓冲，可并行处理不同文档。XLang 包装会把构建结果立即转成语言字符串并释放 C 缓冲。

`_n` 解析只读取指定的字节范围，不要求输入以 NUL 结尾，也不复制整份字幕；返回的 cue 字符串仍由 XRT 独立持有，调用方在解析结束后可以释放原输入。

三个显式长度 C 入口 `xsrt_parse_n`、`xsrt_parse_vtt_n`、`xsrt_parse_time_n` 会在读取输入前拒绝 `SIZE_MAX`；解析入口返回错误值并设置 XRT 诊断，时间入口返回 -1。其他长度必须真实对应可读缓冲区。

公开 C 操作在调用开始时清除本线程遗留的 XRT 错误；成功返回时不会同时留下上一调用的错误。本次输入、内存或格式错误仍按原返回值和 `xrtGetError()` 报告。

时间解析结果与格式化输入均使用 `int64_t`，与 XLang `int` 的 C ABI 一致。实现源直接包含公开头，避免两套声明再次漂移。

## 范例与包级验证

2026-10-14：修复无效 UTF-8 的 C 输入被解析为正常字幕、无效 UTF-8 cue 被生成的问题，SRT/WebVTT 解析和生成三条红绿负例通过。`test/check.ps1` 覆盖严格 GCC、C/XL 边界及双语同输出；`test/check.sh` 对 C 边界启用 Linux ASan/UBSan 与泄漏检查，两套门禁均通过。完整 AOT 阻断不变。

- XLang：`release/examples/extlibs/srt.xl`；与 C 同输出：`release/examples/extlibs/srt_pair.xl` 和 `release/examples/extlibs/srt_c.c`
- 双语对照范例同时展示 SRT 解析/生成、带 ID 与设置的 WebVTT 解析/生成，
  以及时间戳解析/格式化；WebVTT 写出会规范化为含小时的时间格式。
  两份范例各自断言固定的 SRT/WebVTT 文本与毫秒时间，不只互比输出。
- C 边界：`release/extlibs/srt/test/test_srt.c`
- XLang 边界：`release/extlibs/srt/test/boundaries.xl`、`demo6/test/cases/extlib_srt.xl`
- 分配失败：`release/extlibs/srt/test/allocations.xl`；逐个注入前 32 个分配点，核对四条公开解析/生成路径的 `MemoryError` 分类、成功恢复和语义往返。

在 `demo6/release` 执行 `xl run extlibs/srt/test/boundaries.xl`；在 `demo6` 执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package srt -Source release/extlibs/srt/test/test_srt.c`。验证覆盖严格时间戳、CRLF、WebVTT ID/设置往返、异常、超过旧固定缓冲长度的文本及独立结果。
Windows `test/check.ps1` 同时运行中央 XLang 用例、包内边界和分配失败用例，并将 C/XLang 范例输出逐字比较；Linux `test/check.sh` 还以 ASan/UBSan 运行 C 边界。
Windows 默认门禁还构建、运行核心 AOT，再构建、运行带输出的双语范例并核对
输出；`-SkipAot` 只跳过这两项 AOT。构建产物位于每次独占的 `build/test`
子目录，退出时校验路径并清理。2026-10-20 当前发布物的核心 AOT 通过，
带输出范例构建缺 `xl6ValueDataSnapshotV1`，默认门禁正确返回失败；
聚焦 C/XLang 门禁及 Linux ASan/UBSan、双语对照均通过。

2026-10-13：追加 `int64` 上界/越界、WebVTT `NOTE\t` 与保留 ID 的 C/XL 回归；Windows C/XL、Linux 严格 GCC 与 ASan/UBSan 已通过。完整 AOT 构建仍受共享 `xl6ValueIndexReadCopyV1` / `xl6ValueDataSnapshotV1` 链接缺符号影响；包级通过不等于发布验收完成。

2026-10-14：WebVTT 设置里出现第二个 `-->` 原先解析成功，但同一 cue 无法由生成器输出。新增 C/XLang 红色复现后在解析阶段拒绝，Windows 包级严格 GCC、C/XL 边界及双语同输出门禁、Linux ASan/UBSan 均通过；官网详情源和生成页哈希一致。完整 AOT 主线阻断不变。

2026-10-17：孤立回车会使解析结果无法再次生成；新增 C/XLang 负例后，SRT/WebVTT 解析统一只接受 LF/CRLF。Windows 严格 GCC、C/XL 边界、双语同输出和 Linux ASan/UBSan 包级门禁通过；完整 AOT 主线阻断不变。

2026-10-14：公开 C 入口补失败后成功的错误槽清理；`parseTime`、`formatTime`、`build` 和 `parse` 连续调用先复现旧错误残留，修复后 C 回归、Windows 严格 GCC/C/XL 包级门禁和 Linux ASan/UBSan 均通过。完整 AOT 的共享缺符号阻断不变。

2026-10-15：WebVTT 的长 `id/settings` 写入缓冲失败原被误报为无效 cue；分配故障红绿回归后，`buildVtt` 保留 `XERR_MEMORY`，无效 cue 仍报 `XERR_VALUE`。测试覆盖生成前 5 个和解析前 24 个分配点。包清单声明 Buffer、Value Container、Unicode 模块；同一核心 AOT 从 3,611,648 降至 656,384 字节并运行通过。Windows 严格 GCC/C/XLang 双语门禁、Linux ASan/UBSan 复跑通过；完整边界 AOT 仍缺 `xl6ValueIndexReadCopyV1`，不计为发布完成。

2026-10-15：箭头偏移改用 `size_t`，不再把长行位置收窄为 `int`；Windows 严格 GCC/C/XLang、双语输出对照和 Linux ASan/UBSan 门禁通过。现有回归覆盖 70 KiB 正文，不把它冒充数 GB 输入实测；完整 AOT 主线阻断不变。

2026-10-17：Linux `test/check.sh` 加入中央与包内 XLang 用例、C/XLang
范例逐字输出对照。严格 GCC 首次发现单文件 C 范例在首次包含 XRT 时
仅开启 Value Container，随后包含实现源所需的 Buffer/Unicode 声明
不可见；范例现于公开头之前声明这两个模块，不重复链接实现源。
修复后 Linux C ASan/UBSan 与语言联合门禁、Windows C 范例均通过。
完整 AOT 共享主线阻断不变。

2026-10-18：C 边界新增 UTF-8 BOM、CRLF 与中文正文的解析、规范化写出、
再次解析正例，与既有 XLang 中文边界对齐。Windows 严格 GCC/C/XLang
及双语范例门禁、Linux C ASan/UBSan 与 XLang 联合门禁通过；
实现未改，完整 AOT 共享阻断不变。
