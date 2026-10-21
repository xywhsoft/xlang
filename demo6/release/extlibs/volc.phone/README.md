# volc.phone

火山引擎云手机 ACEP 的 XLang 客户端草案，复用 `volc.core` 的签名和请求能力。当前提供请求体构造、资源/任务操作、响应查询及 `Client` / `Response` 类；`raw_spec` 和 `call_spec` 接收调用方编写的逐行声明。包内原生 C 文件仅提供字符串切片辅助函数，**不是**等价的 C 客户端接口。

## 当前状态

**未达到发布验收，不应用于生产请求。** 现行 `xl.exe` 仅检查 `import volc.phone` 就会在独立维护的 `xhttp.multipart` 的 `Buffer.appendBytes` 调用处失败；因此现有 XLang 范例、在线请求和 AOT 均未通过。复现：

```powershell
cd demo6
.\release\xl.exe check dev\extlib_blockers\volc_phone_import_chain.xl
```

这个依赖阻断不能通过包内兼容回退掩盖。依赖恢复后，本包还需要完成以下自身问题，才能进入发布验收：

- 响应助手目前按文本搜索 JSON；转义引号、嵌套同名字段及字符串中的键名可能产生错误结果。必须改用结构化 JSON 解析，再用离线响应夹具验证。
- 字符串搜索使用 UTF-8 字符索引，原生 `x_volc_phone_substr` 使用字节索引；非 ASCII 响应可能在字符中间截断。应统一为 XLang 的字符切片并删除原生辅助函数。
- 原生辅助函数的八槽结果缓存为进程共享且未同步，不能作为线程安全接口。删除辅助函数后同时移除这套生命周期机制。
- 定型请求把凭据与 JSON 请求体拼入逐行 spec；值中出现换行会改变声明边界。定型入口应直接调用 `volc.core` 的定型请求/预览 API，`*_spec` 只留给明确使用该格式的调用方。

更细的复现与修复顺序见 [`dev/extlib_blockers/volc_phone_import_and_response.md`](../../../dev/extlib_blockers/volc_phone_import_and_response.md) 和 [`dev/extlib_blockers/volc_phone_import_chain.md`](../../../dev/extlib_blockers/volc_phone_import_chain.md)。仓库已有离线 [XLang 用例](../../examples/extlibs/volc_phone.xl)，但当前不能执行；C 对照范例、mock HTTP 成功/失败、结果所有权及真实服务验收尚未完成。测试凭据不得替换为真实密钥并提交到仓库。
