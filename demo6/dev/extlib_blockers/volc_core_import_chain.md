# volc.core 导入链被 xhttp.multipart 阻断

最小复现：从 `demo6` 目录运行：

```powershell
.\release\xl.exe check dev\extlib_blockers\volc_core_import_chain.xl
.\release\xl.exe run dev\extlib_blockers\repro_buffer_append_bytes.xl
.\release\xl.exe run test\cases\extlib_volc_core.xl
```

当前 `import xhttp.client` 会解析到 `release/extlibs/xhttp.multipart/src/xlang/xhttp_multipart.xl`，在第 352 行 `stable.appendBytes(source, c_size(string.byteLen(source)))` 报 `[sem] unknown class/union field`。`volc.core` 的 C 侧签名 smoke 能通过，但 xlang 侧在导入依赖时停止，尚未测试核心请求能力。

当前内建 `Buffer` 公开的是 `append(string text, int size = -1)`、`append(bytes data, int size = -1)` 和 `append(Buffer source, int size = -1)`，没有 `appendBytes` 成员。新增的两行直接复现也在成员解析时报错；仓库现有 `release/examples/stdlib_buffer.xl` 同样使用旧名而失败。建议由负责 `xhttp.multipart` 的线程按真实 Buffer ABI 改为 `append(source, string.byteLen(source))`，并覆盖 `import xhttp.client`、`import volc.core` 的最小导入与执行测试。`stdlib_buffer.xl` 范例也需同步。需确认错误信息不再出现后，才恢复本包压实；本任务不修改该依赖或 xlang 主线。

2026-10-21 复核：此前记录的 `available()` ABI 声明差异已经在当前源码中解决。C 头与实现均返回 `int`，XLang `extern` 声明为 `c_int`，公开的 `available()` 显式比较非零值并返回 `bool`。这只证明静态声明一致；`xhttp.multipart` 导入恢复后，仍须运行 C/XLang 对照与 Linux LP64 导入门禁，才能验收实际调用链。
