# volc.phone

火山引擎云手机 ACEP 的 XLang 客户端草案，复用 `volc.core` 的签名和请求能力。当前提供请求体构造、资源/任务操作、响应查询及 `Client` / `Response` 类；`raw_spec` 和 `call_spec` 接收调用方编写的逐行声明。本包不再有原生 C 辅助函数，C 客户端能力不由此包提供。

## 当前状态

**未达到发布验收，不应用于生产请求。** 2026-10-24 用现行
`release/xl.exe` 检查 `import volc.phone`，首先在独立维护的
`xhttp.body/src/xlang/xhttp_body.xl:59` 报未知类型 `cstring`，随后
`xhttp.client` 也报同类错误；旧记录中的 `xhttp.multipart` / `appendBytes`
不再是当前首个阻断。因此现有 XLang 范例、在线请求和 AOT 均未通过。复现：

```powershell
cd demo6
.\release\xl.exe check dev\extlib_blockers\volc_phone_import_chain.xl
```

这个依赖阻断不能通过包内兼容回退掩盖。依赖恢复后，本包还需要完成以下自身问题，才能进入发布验收：

- 响应助手目前按文本搜索 JSON；转义引号、嵌套同名字段及字符串中的键名可能产生错误结果。必须改用结构化 JSON 解析，再用离线响应夹具验证。
- 定型请求把凭据与 JSON 请求体拼入逐行 spec；值中出现换行会改变声明边界。定型入口应直接调用 `volc.core` 的定型请求/预览 API，`*_spec` 只留给明确使用该格式的调用方。

原有字节偏移切片和进程共享八槽缓存已移除，改用 XLang 字符切片；
中文离线用例已添加到范例，待导入链恢复后执行验证。

更细的复现与修复顺序见 [`dev/extlib_blockers/volc_phone_import_and_response.md`](../../../dev/extlib_blockers/volc_phone_import_and_response.md) 和 [`dev/extlib_blockers/volc_phone_import_chain.md`](../../../dev/extlib_blockers/volc_phone_import_chain.md)。仓库已有离线 [XLang 用例](../../examples/extlibs/volc_phone.xl)，但当前不能执行；C 对照范例、mock HTTP 成功/失败、结果所有权及真实服务验收尚未完成。测试凭据不得替换为真实密钥并提交到仓库。
