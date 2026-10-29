# ZIP 桥接错误先于 ArchiveError 被抛出

- 验证时间：2026-10-15，当前 `demo6/release/xl.exe`。
- 最小复现：从 `demo6` 执行 `release/xl.exe run dev/extlib_blockers/zip_error_dispatch.xl`。
- 期望：`zip.ZipArchive.add` 拒绝 `../escape.txt`，由 `src/zip.xl` 抛出的 `ArchiveError` 被调用方捕获，进程退出码为 0。
- 实际：原生 `xzip_archive_add` 返回 `false` 并发布 XRT 错误后，运行时直接报告未捕获的 `IOError: unsafe ZIP entry path: ../escape.txt`；`catch ArchiveError` 未执行。
- 影响：`test/cases/extlib_zip.xl` 的路径安全负例无法按公开包装层的异常类型捕获。不能把中央用例简单改为捕获 `IOError`，否则会掩盖 C/XLang 桥接的错误类型不一致。
- 调查线索：`release/extlibs/zip/zip_native.c` 在失败时调用 `xzip_publish_error`，将不安全路径映射为 `XERR_PERMISSION`；`src/runtime/xl6_error_text.c` 对该类非文件域原生错误返回 `IOError`。`release/extlibs/zip/src/zip.xl` 则在 `addNative` 返回 `false` 后显式 `throw ArchiveError __zip_error()`，但此分支未能覆盖先到的原生异常。请检查 extern 调用返回时的自动错误转换；若该行为是有意设计，需要提供显式处理原生错误的桥接方式，并统一包侧公开异常合同。
- 历史状态：blocked-upstream；当时未修改编译器或压制原生诊断。

2026-10-25 复验：当前 Windows 发布版可在 extern 声明上使用
`@no_raise`。ZIP 包对显式检查返回值的原生布尔入口采用该声明，
在包装层将一般归档错误转换为 `ArchiveError`，并通过原生错误类别查询
保留内存故障的 `MemoryError`。上述最小复现退出码为 0；包内七种
失败路径及内存故障用例在 JIT/AOT 均通过。此阻断已由包侧解除。
Linux 发布版及其他扩展包不由这一结果自动签收。
