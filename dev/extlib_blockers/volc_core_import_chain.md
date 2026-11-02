# volc.core HTTP 导入链阻断

2026-11-02 用当前已发布的 `release/xl.exe` 再验：检查
`release/extlibs/volc.core/src/volc_core.xl`、最小 `import volc.phone`
及公开 `volc_sts.xl`，三条路径均退出1，仍停在独立维护的
`xhttp.client/src/xlang/xhttp_client.xl`。首组诊断为889/903行构造
重载、946/969行 `NetEngine.nativeHandle`、956/963行 `TlsConfig`。
没有进入三个包的网络请求，不能以离线签名/响应成功替代完整导入。
原始命令、输出与发布物指纹见
[`extlib-volc-import-recheck-20261102.json`](../../docs/reports/extlib-volc-import-recheck-20261102.json)。

恢复条件：由HTTP维护上下文按当前网络类型的真实构造、句柄访问和TLS
配置声明同步客户端，回归 `import xhttp.client` 与三个上层包，然后
补齐包级 mock HTTP、超时/失败、C/XLang对照。普通包不添加旧接口别名，
不换私有实现或绕过导入。以下multipart诊断保留为旧发布物历史记录，
不是2026-11-02的第一阻断。

最小复现：从 `demo6` 目录运行：

```powershell
.\release\xl.exe check dev\extlib_blockers\volc_core_import_chain.xl
.\release\xl.exe run dev\extlib_blockers\repro_buffer_append_bytes.xl
.\release\xl.exe run test\cases\extlib_volc_core.xl
```

当前 `import xhttp.client` 会解析到 `release/extlibs/xhttp.multipart/src/xlang/xhttp_multipart.xl`，在第 352 行 `stable.appendBytes(source, c_size(string.byteLen(source)))` 报 `[sem] unknown class/union field`。`volc.core` 的 C 侧签名 smoke 能通过，但 xlang 侧在导入依赖时停止，尚未测试核心请求能力。

当前内建 `Buffer` 公开的是 `append(string text, int size = -1)`、`append(bytes data, int size = -1)` 和 `append(Buffer source, int size = -1)`，没有 `appendBytes` 成员。新增的两行直接复现也在成员解析时报错；仓库现有 `release/examples/stdlib_buffer.xl` 同样使用旧名而失败。建议由负责 `xhttp.multipart` 的线程按真实 Buffer ABI 改为 `append(source, string.byteLen(source))`，并覆盖 `import xhttp.client`、`import volc.core` 的最小导入与执行测试。`stdlib_buffer.xl` 范例也需同步。需确认错误信息不再出现后，才恢复本包压实；本任务不修改该依赖或 xlang 主线。

2026-10-21 复核：此前记录的 `available()` ABI 声明差异已经在当前源码中解决。C 头与实现均返回 `int`，XLang `extern` 声明为 `c_int`，公开的 `available()` 显式比较非零值并返回 `bool`。这只证明静态声明一致；`xhttp.multipart` 导入恢复后，仍须运行 C/XLang 对照与 Linux LP64 导入门禁，才能验收实际调用链。
