# volc.sts

`volc.sts` 通过 `volc.core` 提供 AssumeRole 请求和签名预览。预览只计算
`Action=AssumeRole`、查询参数和授权头，不发网络请求；示例使用固定日期与假凭据。
在线 `assume_role*` 才连接云服务，当前没有完成 mock/沙箱与真实凭据验收。

```xlang
import volc.sts

string preview = volc.sts.preview_assume_role(
    "AKID", "SECRET", "123456789", "demo", "demo-session",
    900, "20240102T030405Z")
```

`preview_assume_role_spec` / `assume_role_spec` 使用逐行 `key=value` 声明；
`preview_assume_role` / `assume_role` 根据账号和角色名构造角色 TRN；
`assume_role_trn` 接收完整 TRN。请求失败原因由 `last_error()` 查询。
直参接口直接调用 `volc.core` 的定型 STS 入口，不再把值重新拼成声明文本；
角色 TRN 和会话名中的 CR/LF 在发请求或签名前拒绝，避免生成额外查询项。
`*_spec` 入口仍是调用方自行编写的逐行声明，不适合拼接未经验证的值。
`json_string_field`、`credentials_json` 等响应助手目前按文本切分 JSON，
不能正确处理转义引号、嵌套同名字段等一般 JSON 响应；完成结构化解析和
离线响应测试前，不应将它们用于不可信响应。

## C 对照

C 调用方包含依赖包的 `volc_core_native.h`，直接调用
`x_volc_core_sign_preview_sts` / `x_volc_core_sign_preview_sts_spec`；
这是 XLang STS 包使用的同一原生实现，不另造一套签名器。返回字符串由
`volc.core` 的当前线程结果槽持有，长期保存或跨线程使用前须复制。
`examples/assume_role.c` 展示离线预览，`test/test_sts.c` 验证直参/声明
文本结果一致、URL 查询编码、无效日期拒绝与随后成功调用的错误恢复。

在 `demo6` 目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/volc.sts/test/check.ps1
```

该门禁包含 Windows 严格 GCC 语法、C 边界及 C 范例，**不发网络请求**。
Linux 可运行 `bash release/extlibs/volc.sts/test/check.sh`，以 ASan/UBSan
复验同一离线 C 路径。Linux 测试宿主为了与 HTTP 依赖头的编译特性一致
启用完整 XRT 模块，链接时丢弃未用代码；测试宿主不在包 manifest 中，
不增加发布产物的代码量。
直参签名和请求入口的换行拒绝、失败后成功预览的恢复已进入 C 边界门禁；
XLang 直参包装改为直接调用定型入口，尚待依赖导入恢复后运行验证。
2026-10-24 用现行 `release/xl.exe check release/examples/extlibs/volc_sts.xl`
复验，首先在独立维护的 `xhttp.body/src/xlang/xhttp_body.xl:59` 报未知
`cstring`，`xhttp.client` 同样不认识该类型；旧记录中的
`xhttp.multipart` / `Buffer.appendBytes` 不是当前首个阻断。原始复现见
`dev/extlib_blockers/volc_sts_xhttp_multipart_import.md`。依赖修复后还需
重跑 XLang 范例、补结构化 JSON 响应解析、mock/沙箱请求、C/XL
同输出及完整 AOT。本包尚未达到发布验收。
