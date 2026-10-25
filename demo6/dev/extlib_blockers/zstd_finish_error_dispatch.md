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

2026-10-24 当前发布版复验：`release/xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
`xl run dev/extlib_blockers/zstd_finish_error_dispatch.xl` 仍抛
`RuntimeError: zstd decompressor already finished`、退出 1；
`xl run dev/extlib_blockers/zstd_truncated_error_dispatch.xl` 仍抛
`IOError: zstd stream is truncated or empty`、退出 1。应修复主线的
`extern` 自动错误派发与包层转换合同，再复跑两条负例及完整范例；
本任务暂不改 zstd 公开错误类型。

2026-10-24 新 Windows 发布版再次复验：完成后写入最小用例仍抛
`RuntimeError: zstd decompressor already finished`，截断帧最小用例仍抛
`IOError: zstd stream is truncated or empty`，两者均退出 1。
完整教学范例的 JIT 和独立 EXE 均在截断帧负例同样退出，尚未执行后续损坏帧
断言；包级基础/固定帧/串联/文件/进度五条 AOT 路径通过不解除此阻断。

2026-10-25：ZIP 已验证的 `@no_raise` 现在可用于 zstd 包中由包装层
明确检查返回值的一次性及流对象入口。两个原最小复现均从
`demo6/release` 工作目录以退出码 0 运行；新增包内
`test/error_dispatch.xl` 同时覆盖压缩器完成态和流式截断，JIT 与
独立 EXE 通过，完整教学范例亦通过。文件进度回调入口不能简单
使用 `@no_raise`：实测会将用户回调的 `ValueError` 吞成泛化的
`CompressionError`；已保留原自动派发路径。默认包门禁仍受独立的
工作目录敏感 VFS 头文件查找问题阻断，因此本记录的两个错误分支
已修复，不代表整包发布验收完成。

2026-10-26 当前发布物复验：Windows 默认包门禁、两个原最小复现、
独立标准帧互通及从 `demo6` 根目录运行基础范例均退出 0；Linux
包门禁含 ASan/UBSan、JIT、八条独立 EXE AOT，亦退出 0。本记录
中的错误派发与工作目录阻断均为历史现象；文件回调继续沿用原生
错误派发，不以 `@no_raise` 吞掉用户异常。
