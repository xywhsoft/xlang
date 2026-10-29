# EPUB 复用 XRT CRC32 时的运行宿主符号阻断

- 验证时间：2026-10-17；使用当前 `release/xl.exe` 和同工作树 `lib/xrt/xrt.h`。
- 最小复现：从 `demo6` 运行 `release/xl.exe run dev/extlib_blockers/epub_xrt_crc32_symbol.c --inc-path lib/xrt`。
- 期望：XRT 公开的 `xrtCrc32("123456789", 9)` 返回 `0xcbf43926`，进程退出 0。
- 实际：JIT 报 `tcc: error: unresolved reference to 'xrtCrc32'`。同一文件使用 `xl build ... -o build/test/epub_crc_aot.exe` 后运行退出 0。
- 代码线索：`src/api/xl_xrt_symbols.inc` 已列出 `xrtCrc32`，`src/compile/xl6_artifact_native_symbols.inc` 也登记 `XRT_MODULE_CRC32`，而当前发行版运行宿主没有解析此符号。需核对发行物是否按当前 XRT/符号表重建，以及 `xl run` 对公开 XRT 符号的注册和实际可见性；不可仅凭声明存在就认为 JIT 可调用。
- 影响：EPUB 本可删除包内逐 bit CRC32，直接复用 XRT 的公共实现；但这种替换会令当前发行版 `xl run` 和包级 C 测试链接失败。已撤回本次尝试，保留原有可运行实现，不在扩展库任务修改 xlang 主线。修复宿主后再替换，并复跑 C/XLang、Linux sanitizer、ZIP/XML 独立校验及 AOT 体积对照。
- 状态：blocked-upstream，仅针对 CRC32 复用优化；EPUB 原功能仍可运行，其完整 AOT 还有独立的共享快照符号阻断。
