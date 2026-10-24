# 当前源码驱动导入 xhttp.client 的 TcpStream 构造阻断

最小复现是相邻的 `xhttp_client_tcpstream_source.xl`，仅含
`import xhttp.client`。从 `demo6` 目录使用当前源码构建的驱动执行：

```powershell
pwsh -NoProfile -File tools/build_xlir_public_source_driver.ps1 `
    -Directory build/c-owned/extlib-goal-aot-20261024-2
$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path
build/c-owned/extlib-goal-aot-20261024-2/public_source.exe `
    dev/extlib_blockers/xhttp_client_tcpstream_source.xl run `
    build/c-owned/extlib-goal-aot-20261024-2/xhttp_client_import
```

2026-10-24 的当前源码快照在
`release/extlibs/xhttp.client/src/xlang/xhttp_client.xl:889`
报告 `[sem] 没有匹配的 create 重载`。该处 `takeTcp()` 调用
`TcpStream(stream)`，但 `src/stdlib/xl6_stdlib_tcp.inc` 中的公开
`TcpStream` 构造需要 `NetEngine` 与 `NetAddress`（可附带 options），
没有接管裸 `point` 的构造。`volc.core`、`volc.phone`、`volc.sts`
三个普通包因依赖 `xhttp.client` 在导入阶段同样失败。

旧 `release/xl.exe` 首先不认识该依赖链中的 `cstring`；这与当前源码
的上述构造错误是两个不同阶段的阻断，不能用旧二进制的诊断替代
源码检查结果。

建议由 `xhttp.client` 的独立维护线程确认 `takeResponseNative()` /
`takeTcpNative()` 的裸句柄所有权与 `TcpStream` 当前受管生命周期协议，
提供匹配的内部接管入口并在失败时正确归还或释放句柄；随后以本文件、
三个 Volc 包的单行导入及各自运行用例复验。不要在 Volc 包添加旧构造
回退或伪造 `NetEngine`。本普通扩展库任务不修改 HTTP 包或 xlang 主线。
