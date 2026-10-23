# ZIP 导入与发布编译器的 cstring 识别

最小复现：从 `demo6` 目录执行
`release/xl.exe check dev/extlib_blockers/zip_cstring_release_binary.xl`。
当前发布版在 `release/extlibs/zip/src/zip.xl:108` 报
`[sem] 未知类型: cstring`，尚未运行到 ZIP 的文本所有权移交路径。

ZIP 包在该处声明 `cstring` 参数，用于把已知长度的文本传给原生入口；
当前源码已包含 `cstring` 机制，但不能据此假定发布目录的 `xl.exe`
与源码同步。应由主线构建/发布任务重建编译器并复验导入、JIT、AOT；
普通扩展包任务不将参数退回不带长度的旧字符串 ABI，也不改主线。
