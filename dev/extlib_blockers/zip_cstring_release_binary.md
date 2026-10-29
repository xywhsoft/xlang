# ZIP 导入与发布编译器的 cstring 识别

历史最小复现：从 `demo6` 目录执行
`release/xl.exe check dev/extlib_blockers/zip_cstring_release_binary.xl`。
旧发布版曾在 `release/extlibs/zip/src/zip.xl:108` 报
`[sem] 未知类型: cstring`。

ZIP 包在该处声明 `cstring` 参数，用于把已知长度的文本传给原生入口；
2026-10-25 复验：当前 Windows `release/xl.exe check` 通过；ZIP 默认
`test/check.ps1` 的导入、JIT 及三个 AOT 目标也通过。此阻断已解除。
Linux `release/xl` 仍是较早版本，本次没有在 Linux 复验新包。
