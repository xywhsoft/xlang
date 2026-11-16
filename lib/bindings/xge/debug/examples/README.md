# Debug capabilities

`capabilities.xl` prints the engine version and platform/graphics capability
report without starting a window or GPU. Use the matched debug SDK, built with
`XGE_DEBUGMODE=1`; the ordinary SDK deliberately does not publish this module.

```text
xl run capabilities.xl
xl build capabilities.xl -o capabilities.exe
```

`debugDumpCaps(Buffer)` writes into the existing buffer and returns the native
status. `XGE_ERROR_BUFFER_TOO_SMALL` means its NUL-terminated report was
truncated; enlarge the buffer and call again. It does not grow the buffer or
silently convert a partial report into a successful string. The example uses
8192 bytes, checks success, and explicitly removes the unused NUL tail before
printing. An initialized engine's debug calls belong on its creator thread.

The buffer is managed by xlang. No window, file, network service or manual
native pointer cleanup is needed.

`live_stats.xl` 打开窗口，清屏并创建一张纹理，打印真实纹理数量和占用字节，
随后关闭纹理、结束这一帧并关闭引擎。它显式导入 `xge` 和 `xgeDebug`，图形
启动后的查询在创建引擎的线程执行。`DebugStats` 是独立数值快照，不拥有纹理。
若资源仍被持有，关闭引擎会 checked 报 Busy，应先释放资源，再重试关闭。

```text
xl run live_stats.xl
xl build live_stats.xl -o live_stats.exe
```
