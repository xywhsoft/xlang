# volc.phone

2026-10-27 离线响应边界收口：`Response` 模型与 JSON 解析从网络
入口拆出。响应只从 `ResponseMetadata` 读取 `RequestId`、`Error`
及其 `Code/CodeN/Message`；`Result` 由 JSON 库序列化，不再截取
原文尾部。公开字段助手只读 `Result` 的直接字段，或调用方传入的
结果对象直接字段；缺失返回空串/零，类型错误抛 `ValueError`。
`task_state` 依据真实整数 `TaskResult` 和元数据错误判定，错误返回
`failed`，可让 `wait_task` 结束。Windows/Linux 离线 JIT/AOT 用例
见 `test/check_response.py`，覆盖对象模型、转义和畸形输入。
整包导入仍在独立维护的 `xhttp.client` 失败，网络、C 对照和完整
XLang 运行尚未验收，不能把离线通过记作发布完成。响应结构遵循
[云手机 OpenAPI 调用说明](https://docs.volcengine.com/docs/Cloudphone/OpenAPICallInstructions?lang=zh)。

2026-10-26 最小导入复验仍停在 `xhttp.client`；除构造器和
`NetEngine.nativeHandle()`，还报 `TlsConfig` 未知类型。
详见 `../../../dev/extlib_blockers/volc_phone_import_chain.md`。
本包尚未进入自身语言与在线验收，不增加包侧兼容回退。

2026-10-25 当前发布版已越过旧 `cstring` 未知类型诊断，但
`xl check dev/extlib_blockers/volc_phone_import_chain.xl` 仍在
`xhttp.client` 的 `TlsStream` 构造和 `NetEngine.nativeHandle()` 成员
解析处失败。重新构建的 Windows `xl.exe` 已能运行
`examples/hello.xl`；全局 ownership catalog 故障不再是当前阻断。
该包仍不能进入 XLang 运行验收。

火山引擎云手机 ACEP 的 XLang 客户端草案，复用 `volc.core` 的签名和请求能力。当前提供请求体构造、资源/任务操作、响应查询及 `Client` / `Response` 类；`raw_spec` 和 `call_spec` 接收调用方编写的逐行声明。本包不再有原生 C 辅助函数，C 客户端能力不由此包提供。

## 当前状态

**未达到发布验收，不应用于生产请求。** 重新构建的 Windows
`release/xl.exe` 检查 `import volc.phone`，当前首先在独立维护的
`xhttp.client` 中报告 `TlsStream` 构造无匹配重载，以及
`NetEngine.nativeHandle()` 成员不可解析。早先的 `cstring`、
`xhttp.multipart` / `appendBytes` 诊断均是旧发布物记录。
因此现有 XLang 范例、在线请求和 AOT 均未通过。复现：

```powershell
cd demo6
.\release\xl.exe check dev\extlib_blockers\volc_phone_import_chain.xl
```

这个依赖阻断不能通过包内兼容回退掩盖。依赖恢复后，本包还需要完成以下自身问题，才能进入发布验收：

- 定型请求把凭据与 JSON 请求体拼入逐行 spec；值中出现换行会改变声明边界。定型入口应直接调用 `volc.core` 的定型请求/预览 API，`*_spec` 只留给明确使用该格式的调用方。

原有字节偏移切片和进程共享八槽缓存已移除，改用 XLang 字符切片；
中文离线用例已添加到范例，待导入链恢复后执行验证。

更细的复现与修复顺序见 [`dev/extlib_blockers/volc_phone_import_and_response.md`](../../../dev/extlib_blockers/volc_phone_import_and_response.md) 和 [`dev/extlib_blockers/volc_phone_import_chain.md`](../../../dev/extlib_blockers/volc_phone_import_chain.md)。仓库已有离线 [XLang 用例](../../examples/extlibs/volc_phone.xl)，但当前不能执行；C 对照范例、mock HTTP 成功/失败、结果所有权及真实服务验收尚未完成。测试凭据不得替换为真实密钥并提交到仓库。
