# volc.phone 的 HTTP 导入链阻断

2026-10-26 当前发布物复验：从 `demo6/release` 分别执行
`./xl.exe check ../dev/extlib_blockers/volc_phone_import_chain.xl`、
`./xl.exe check extlibs/volc.core/src/volc_core.xl`，
以及 `./xl.exe check examples/extlibs/volc_sts.xl`，均停在独立维护的
`xhttp.client/src/xlang/xhttp_client.xl`。889、903 行的 `TcpStream` /
`TlsStream` 构造无匹配重载，946、969 行的
`NetEngine.nativeHandle()` 无匹配成员；956、963 行还报告
`TlsConfig` 未知类型。三个上层包均未进入自身语义或在线调用。
HTTP 维护上下文须按当前网络类型声明核对这三组 API，并以
`import xhttp.client`、三个上层导入和包级回归验收；不能在 Volc
包装层添加旧构造器、成员或类型的兼容回退。下文的 multipart
和 cstring 错误均为旧发布物记录，不是当前首个阻断。

2026-10-25 当前发布物更新：从 `demo6/release` 执行
`xl.exe check ..\dev\extlib_blockers\volc_phone_import_chain.xl`，
旧 `xhttp.multipart`/`cstring` 诊断已不复现；现于独立维护的
`xhttp.client/src/xlang/xhttp_client.xl:889,903` 报 `create` 无匹配重载，
并于 946、969 行报 `NetEngine.nativeHandle` 成员不可解析。
`xl.exe check extlibs/volc.phone/src/phone.xl` 得到相同诊断，尚未进入
`volc.phone` 自身语义。应由 HTTP 依赖的维护上下文按当前网络类签名
修复并回归这两条导入路径；本普通扩展库任务继续跳过，不添加包内
兼容回退。下文是旧发布物的故障历程，不代表当前首个诊断。

最小复现：从 `demo6` 目录运行：

```powershell
.\release\xl.exe check dev\extlib_blockers\volc_phone_import_chain.xl
```

期望：仅导入 `volc.phone` 的程序通过检查。实际：当前发布物在 `release/extlibs/xhttp.multipart/src/xlang/xhttp_multipart.xl:352` 报 `[sem] unknown class/union field`。链路为 `volc.phone -> volc.core -> xhttp.client -> xhttp.multipart`，不是 `volc.phone` 的请求体或响应解析代码触发。

根因和修复方向与 [`volc.core` 导入阻断](volc_core_import_chain.md)相同：由独立维护 `xhttp.multipart` 的上下文按现行 `Buffer` API 修复 `appendBytes` 调用，并回归 `import xhttp.client`、`import volc.core`、`import volc.phone`。本任务不在 `volc.phone` 中增加兼容绕过，也不修改独立维护的依赖包。修复后再运行 `release/examples/extlibs/volc_phone.xl` 与 C/XL 对照测试。

2026-10-16 使用当前 `release/xl.exe check` 复验，仅导入仍在
`xhttp_multipart.xl:352` 报 `Buffer.appendBytes` 无匹配成员。阻断未解除；
`volc.phone` 的 JSON 与 Unicode 整体修复继续等待依赖链恢复后验收。

2026-10-17 使用当前 `release/xl.exe check` 再验，仅导入仍停在
`xhttp_multipart.xl:352` 的 `Buffer.appendBytes` 无匹配成员；本任务不修改
独立维护的依赖包，也不将未运行的 `volc.phone` 语言路径记为通过。

2026-10-17 再次复验 `xl check release\extlibs\volc.phone\src\phone.xl`，
仍在同一依赖位置失败。静态审计另发现包内
`x_volc_phone_native.c` 的 `x_volc_phone_substr` 把结果放入进程共享的
8 槽指针数组，轮转时立即释放旧槽；`g_xvolc_phone_lock` 仅创建，
从未用于保护槽位或返回值。跨线程调用可能在 XLang 复制返回文本前
释放另一个线程仍在读取的缓冲。此风险尚未通过并发运行验证，不能
把静态判断写成已复现的崩溃。依赖导入修复后，先以离线并发和 Unicode/
转义 JSON 用例固定问题，再改用 XRT JSON 结构化解析并删除这份仅服务
手写切片的 C helper；不要靠给共享环加锁保留不必要的生命周期机制。

2026-10-18 再以当前 `release/xl.exe run examples/extlibs/volc_phone.xl`
复验，仍在 `xhttp_multipart.xl:352:23` 报 `[sem] unknown class/union field`，
未进入 `volc.phone` 的示例正文。清单核对显示该包也是当前普通队列中
唯一缺少 C 对照范例和包级 README/官网详情源的包；依赖链恢复后应将
这些交付物与 JSON/并发修复一起验收，不把当前未执行的语言路径记为通过。

2026-10-20 用现行 `release/xl.exe check dev/extlib_blockers/volc_phone_import_chain.xl`
再次复验，仍于 `xhttp_multipart.xl:352:23` 报未知字段，并于 352:34 报
`Buffer.appendBytes` 无匹配成员。阻断状态不变；本任务继续跳过该包，
不改独立维护的 `xhttp.multipart` 或为其添加兼容绕过。

2026-10-24 包内已删除 `x_volc_phone_substr` 和未同步的八槽结果缓存，
改为 `string.slice`，并补中文离线断言。当前 `xl check` 仍在上述
`xhttp.multipart:352` 处失败；包内断言尚未执行，不因此签收包级修复。
