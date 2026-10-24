# Zstd 完成后错误先于包装层异常抛出

- 验证时间：2026-10-19，当前 `demo6/release/xl.exe`。
- 最小复现：从 `demo6` 执行 `release/xl.exe run dev/extlib_blockers/zstd_finish_error_dispatch.xl`。
- 期望：`ZstdDecompressor.update` 在 `finish()` 后拒绝新输入，由 `src/zstd.xl` 将失败转为 `CompressionError`，调用方捕获后退出码为 0。
- 实际：包内 C 入口返回 `false` 并记录 `XRT_ZSTD_ERROR_STATE`；XLang 的 `extern updateNative` 在包装函数检查返回值前抛出未捕获的 `RuntimeError: zstd decompressor already finished`，退出码为 1。
- 影响：公开 XLang 包装层承诺的 `CompressionError` 无法覆盖这类原生错误；若仅在包内改为捕获 `RuntimeError`，会掩盖与 ZIP 等包共有的异常派发边界。不把该负例登记为绿色包门禁。
- 修复建议：在主线审查 `extern` 返回布尔失败并设置 XRT 错误时的自动派发规则。需要一种明确机制，让包层决定是否将原生错误转换为公开异常，同时保留原生错误消息；修复后复跑本复现和 `zip_error_dispatch.xl`，并将本例纳入 zstd 包门禁。
- 状态：blocked-upstream；本任务不修改主线。

2026-10-24 源码驱动复核了同一派发边界的截断帧分支：
`dev/extlib_blockers/zstd_truncated_error_dispatch.xl` 截去合法帧的
最后一个字节，公开 `zstd.decompress` 预期抛 `CompressionError`；
当前源码的 `extern` 在包装层检查空结果前直接抛
`IOError: zstd stream is truncated or empty`。完整教学范例也在这一
负例停止；改抓父类 `IOError` 只能回避合同，不能算修复。可用已构建的
当前源码驱动和 `XLANG_PACKAGE_PATH=release/extlibs` 运行该文件复现，
修复后应同时跑两个最小文件及完整教学范例的 JIT/独立 EXE。
