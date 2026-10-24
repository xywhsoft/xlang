# xhttp.client 导入时 TcpStream(point) 无匹配构造

2026-10-24 使用已构建的源码驱动，在 `demo6` 目录运行：

```powershell
$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path
build/c-owned/extlib-goal-aot-20261024-2/public_source.exe `
    dev/extlib_blockers/xhttp_client_tcp_adopt_import.xl run `
    build/c-owned/extlib-goal-aot-20261024-2/xhttp_client_import
```

预期：只导入包并返回 0。实际：
`release/extlibs/xhttp.client/src/xlang/xhttp_client.xl:889:25`
报 `[sem] 没有匹配的 create 重载`。`volc.core` 和 `volc.phone` 的仅导入
最小文件也在同一位置失败；旧发布版的 `cstring`/`xhttp.multipart`
诊断属于不同编译器及包源码快照，不能混作当前首个错误。

`xhttp.client` 的 `ClientResult.takeTcp()` 从 C 层取得 `point` 后调用
`TcpStream(stream)`，而当前内置 `TcpStream` 仅声明了从
`NetEngine, NetAddress`（可带选项）构造的重载，没有 `point` 收养入口。
修复需要由 HTTP/TCP 维护线程明确原生流指针的所有权、生命周期与
失败清理，再提供公开或受控的收养 API 并回归 `takeTcp/takeTls`，
不能只删掉方法或在 `volc.*` 包内绕开导入错误。随后先复测
`import xhttp.client`，再复测 `volc.core/phone/sts` 的导入和双语调用。

该复现使用较早构建的源码驱动读取当前包文件；本轮重新构建驱动时
并行主线改动触发 `Build inputs changed`，因此尚不能宣称最新整树
已通过一致性构建。本普通扩展库任务不修改独立维护的 HTTP 包或主线。
