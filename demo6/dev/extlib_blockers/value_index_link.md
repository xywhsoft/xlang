# 共享主线阻断：var 索引读取的 AOT 链接缺少 xl6ValueIndexReadCopyV1

- 验证时间：2026-10-12；当前 `demo6/release/xl.exe` 与同目录 DLL。
- 最小复现：在 `demo6/release` 执行 `xl run ../dev/extlib_blockers/value_index_link.xl` 成功；`xl build ../dev/extlib_blockers/value_index_link.xl -o ../build/test/value_index_probe.exe` 失败。
- 期望：动态数组索引读取返回第一个值，AOT 与 JIT 行为相同。
- 实际：TCC 报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`。
- 影响：`csv` 包的独立 C 测试、454 条 Python 对照、XLang JIT 都通过，但 `tools/check_extlib_csv.ps1` 在 `extlib_csv-build` 停在此符号，无法签收发布级 AOT。
- 线索：C emitter 在 `src/cgen/xl6_ir_cgen_shared_collection.c` 引用该符号，实现在 `src/runtime/xl6_value_index_read.c`，导出声明在 `src/runtime/xl6_text_symbols.inc`。请核查 AOT 运行时单元选择、依赖闭包、符号导出与宏裁剪，与 `text_snapshot_link.md` 的缺失符号一并排查。
- 状态：blocked-upstream；本任务不修改主线。

2026-10-17 当前 `release/xl.exe` 复核：在 `demo6` 执行
`release/xl.exe build dev/extlib_blockers/value_index_link.xl -o build/test/value_index_probe.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`，退出码 1。
本扩展库任务不通过包内冗余实现或删减索引用例绕过。

2026-10-15 `linalg` 最小复核：从 `demo6` 执行
`release/xl.exe build release/extlibs/linalg/test/aot_core.xl -o build/test/linalg_aot_before.exe`
报同一缺失符号。该用例仅调用标量 `dot2`、`mag2`、`det2`，没有索引语句；
`src/linalg.xl` 的其他公开函数读取原生 `var` 数组结果，导入时仍进入 AOT
链接闭包。建议检查 AOT 运行时符号选择/静态资源表和整模块代码生成的依赖闭包；
不能仅凭调用点不含 `[]` 就判定不需要索引读取符号。本任务不修改主线。

2026-10-13 复核：当前 `demo6/release/xlc.dll` 的导出表中已经能看到
`xl6ValueIndexReadCopyV1`，但在 `demo6/release` 执行
`xl build extlibs/graph/test/boundaries.xl -o ../build/test/graph/boundaries.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`。因此 DLL 导出本身
不足以满足 AOT 产物的链接；应检查生成程序实际使用的运行时对象/静态资源
及其依赖选择，而不只检查 `xlc.dll` 导出。此任务不修改该链接路径。

`shellwords` 的 `release/extlibs/shellwords/test/check.ps1 -SkipAot`
通过严格 GCC、C/XL 边界及范例；去掉 `-SkipAot`，
`xl build extlibs/shellwords/test/boundaries.xl -o ../build/test/shellwords_guard/shellwords_xl_test.exe`
在同一符号处失败。该用例读取 `split` 的动态数组结果，
属于同一个索引 AOT 依赖闭包问题。

`statistics` 的 `release/extlibs/statistics/test/check.ps1 -SkipAot` 已通过严格 GCC、
C/XL 边界与双语黄金输出；去掉 `-SkipAot` 后，
`xl build extlibs/statistics/test/boundaries.xl -o ../build/test/statistics_guard/statistics_xl_test.exe`
报同一缺失符号。该包在 `quartiles`、`linearFit`、`histogram` 等路径读取
动态数组，修复共享 AOT 依赖闭包后重跑完整脚本。
2026-10-13 复核：在 `demo6/release` 执行
`xl build extlibs/statistics/test/aot_stdev.xl -o ../build/test/statistics_guard/statistics_stdev_aot.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`。该短用例只检查极值
总体/样本标准差；`xl run extlibs/statistics/test/boundaries.xl` 已通过。包内
Windows C/JIT 与 Linux C sanitizer 门禁均通过，本任务不修改主线链接链路。

`fix` 的 `release/extlibs/fix/test/check.ps1 -SkipAot` 已通过严格 GCC、
Windows/Linux C 测试、XLang JIT 和 C/XLang 报文逐字对照；去掉
`-SkipAot` 时，`xl build extlibs/fix/test/boundaries.xl -o ../build/test/fix_guard/fix_xl_test.exe`
报同一缺失符号。该用例索引 `parse` 返回的字段数组。修复共享 AOT 链路后
重跑完整包级脚本。

`cbor` 的 `release/extlibs/cbor/test/check.ps1 -SkipAot` 已通过严格 GCC、
XLang JIT、CBOR 固定向量、确定性畸形字节样本及双语输出对照；Linux
ASan/UBSan 同样通过。`xl build extlibs/cbor/test/aot_core.xl -o ../build/test/cbor_guard/cbor_xl_core.exe`
和生成的程序均成功，证明编解码核心 AOT 入口可用；但
`xl build extlibs/cbor/test/boundaries.xl -o ../build/test/cbor_guard/cbor_xl_test.exe`
在动态容器索引读取时仍缺少 `xl6ValueIndexReadCopyV1`。

`bson` 的 `release/extlibs/bson/test/check.ps1 -SkipAot` 已通过严格 GCC、
Windows/Linux C 边界、Linux ASan/UBSan、XL JIT、双语范例逐字对照及
无索引 AOT 核心探针。`demo6/release` 下执行
`xl build extlibs/bson/test/boundaries.xl -o ../build/test/bson_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`；动态字典/数组
索引是触发点。请修复共享 AOT 运行时链接闭包后重跑完整包级脚本。

`json5` 的无输出解析核心 AOT 可运行；`demo6/release` 下执行
`xl build extlibs/json5/test/boundaries.xl -o ../build/test/json5_guard/json5_xl_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`，由解析结果的
动态容器索引触发。包内 Unicode 未加引号键名和空白规范缺口已修复；
`json5/test/check.ps1 -SkipAot -ReferenceDir build/test/json5_ref/src`
已通过 C/JIT/双语范例和 56 例参考实现对照。完整 AOT 待本符号修复后重跑。

`srt` 的 Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和无索引核心
AOT 已通过。在 `demo6/release` 执行
`xl build extlibs/srt/test/boundaries.xl -o ../build/test/srt_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`；触发点是读取
`parse` 返回的 cue 数组/对象。请修复共享 AOT 依赖闭包后重跑该脚本。

`sitemap` 的 Windows/Linux 严格 GCC、Linux ASan/UBSan、XL JIT 和无索引
核心 AOT 已通过。在 `demo6/release` 执行
`xl build extlibs/sitemap/test/boundaries.xl -o ../build/test/sitemap_boundaries.exe`
仍报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`；触发点是读取
`parse` 返回的 URL 数组/对象。请修复共享 AOT 依赖闭包后重跑完整边界脚本。

`rss` 的 C/JIT 边界、Windows 严格 GCC 和 Linux ASan/UBSan 通过；
`demo6` 下执行 `release/xl.exe build test/cases/extlib_rss.xl -o build/test/rss_guard.exe`
仍报同一缺失符号，触发点是读取 `parse` 结果的 item/字段索引。
修复共享 AOT 依赖闭包后重跑此用例；包内无索引核心探针为
`release/extlibs/rss/test/aot_core.xl`。

`graph` 的严格 GCC 四单元编译、C 边界、故障注入、线程测试和 XLang JIT 已通过。
在 `demo6` 运行 `powershell -NoProfile -File tools/check_extlib_graph.ps1`，首个
`extlib_graph-build` 阶段报 `unresolved reference to 'xl6ValueIndexReadCopyV1'`。
最小复现仍为本文件顶部的 `value_index_link.xl`；图算法用例读取动态数组结果，
触发相同 AOT 依赖闭包缺口。主线修复后应重跑完整图包门禁；本任务不改主线。

2026-10-14 复核 `qr`：在 `demo6` 执行
`release/xl.exe build test/cases/extlib_qr.xl -o build/test/qr_core.exe`，
期望生成可执行程序并完成二维码矩阵边界测试，实际在 TCC 链接阶段报
`unresolved reference to 'xl6ValueIndexReadCopyV1'`。该脚本对
`qr.matrix()` 返回的动态矩阵使用 `m[0]`、`m[y][x]` 等索引读取；
最小复现继续使用本记录顶部的 `value_index_link.xl`。包级 C/JIT、
Linux ASan/UBSan 与双语范例通过，不等于当前 AOT 通过。修复共享 AOT
运行时依赖闭包后重跑上述完整脚本；本任务不修改编译器。
