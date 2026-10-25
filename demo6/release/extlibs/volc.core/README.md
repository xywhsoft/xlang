# volc.core

2026-10-25 从当前源码重建发布版后，旧 `cstring` 未知类型已不再是
首个阻断。`xl check` 现在停在独立维护的 `xhttp.client`：
`HttpResult.takeTls()` 的 `TlsStream(stream)` 无匹配构造，以及
`HttpClient.open()` 调用 `NetEngine.nativeHandle()` 无匹配成员。
此外新发布版连 `examples/hello.xl` 的 `xl run` 都在 ownership catalog
初始化失败；详见 `dev/extlib_blockers/ownership_catalog_startup_20261025.md`。
本包 C 离线门禁仍通过，但 XLang/在线路径不能据此签收。

同日修复四个 `*_spec` 入口的字段提取失败路径：任何字段分配失败时，
立即返回空文本，`last_error` 为 `out of memory`，并保留 XRT 的
`XERR_MEMORY`；不会用缺失字段生成签名或继续发请求。Windows C
门禁逐点注入 26 个字段分配失败；Linux 离线 ASan/UBSan 门禁覆盖
两种签名预览的 13 个分配点，不链接在线 HTTP 请求路径。成功预览
也在故障清除后复测。XLang 调用与真实在线请求仍受上面的主线阻断。

火山引擎签名与请求的共享基础包。离线能力包括 SHA-256、HMAC-SHA256、规范化查询、JSON 字符串编码和签名预览；在线请求依赖 `xhttp.client`。`volc.phone` 与 `volc.sts` 复用本包。

```xlang
import volc.core

string query = volc.core.canonical_query("ListApps", "2020-01-01", "Limit=10\nName=x lang")
string preview = volc.core.sign_preview_acep(
    "ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240102T030405Z")
```

`extra_pairs` 是逐行 `key=value` 文本，规范化时按键和值排序并对 UTF-8 字节做百分号编码。`x_date` 为空时使用 XRT 当前 UTC 时间；显式指定时必须是有效的 `YYYYMMDDTHHMMSSZ`，无效日期直接报错，不静默改用当前时间。预览返回含 `query`、`x_date`、`x_content_sha256`、`signed_headers`、`authorization` 的 JSON 字符串，不发网络请求。预览中的凭据只应使用测试值；真实授权头不应写入日志。

`*_spec` 的逐行 `key=value` 声明接受 LF 或 CRLF；CRLF 的行尾 `\r`
不会进入字段值。字段内部的控制字符仍按原规则拒绝。ACEP 和 STS
离线预览的 LF/CRLF 同签名对照已纳入 Windows 与 Linux C 门禁。

`duration_seconds` 与 `timeout_ms` 只有省略或留空时才采用默认值
（分别为 3600 秒和 30000 毫秒）；显式值必须是完整的十进制整数，
且位于 C `int` 范围内。错误文本或越界值返回空结果，并在
`last_error` 中报告 `invalid duration_seconds` / `invalid timeout_ms`，
不会截断数值、悄悄改用默认值，也不会继续发送请求。合法 `spec`
仍沿用原来的签名和请求语义。2026-10-25 C 红绿回归覆盖畸形、空白、
超界与缺省；Windows 严格 C、Linux ASan/UBSan 离线门禁及下游
`volc.sts` 双平台 C 门禁通过。XLang 导入和真实在线路径仍受上述
主线阻断，尚未完成发布验收。

直接拼入规范请求或授权头的方法、主机、路径、服务、区域、访问密钥 ID、会话令牌和内容类型不能包含 ASCII 控制字符（含 CR/LF、TAB 和 DEL）；路径报 `invalid path`，其余字段报 `invalid signed field`。请求体仍可包含换行，查询值由百分号编码路径处理。此校验不代替真实服务的凭据和请求参数验证。

通用 `sign_preview` / `request` 的 `path` 是**未编码、以 `/` 开头**的绝对路径；空值按 `/` 处理。签名和请求 URL 共用按路径片段编码的结果，保留 `/`，将空格、非 ASCII 字节和已有 `%` 分别编码；调用方不要预先对路径做百分号编码。非绝对路径报 `invalid path`。此规则与[火山引擎官方 Go SDK 的规范 URI 处理](https://github.com/volcengine/volc-sdk-golang/blob/main/base/sign.go)一致；包内 C 测试固定了含空格和中文路径的完整签名向量。

## C 接口

声明见 `include/volc_core_native.h`。返回的 `const char*` 由包持有，调用方不得释放；需要跨线程或长期保存时应立即复制。返回槽及 `last_error`/`last_status` 使用 XRT 原生线程局部键，线程退出时清理；不同线程的诊断与返回缓存不会互相覆盖。**这只是离线签名层的线程隔离验证，不是在线客户端的并发认证。**在线 `request*` 会创建并销毁一个 XRT 网络引擎及 HTTP 客户端，尚未验收连接复用、并发和真实服务行为。

`x_volc_core_take_result` 供语言桥接显式消费结果槽：若指针属于当前线程的槽，
直接转交分配；静态失败文本则复制一次。结果统一由 `xrtFree` 释放。旧 C API 的
借用合同不变。在线请求的 XLang 文本包装也直接取走响应结果槽，
不再为整段响应做第二次复制；C 端从 HTTP 借用视图生成独立快照的那一次复制仍必需。
当前公开返回仍是 C 字符串文本语义，内嵌 NUL 会截断 XLang 所见内容；
不能把它当作二进制响应 API 使用。

原生 C 范例在 `examples/signing.c`；XLang 范例在 `release/examples/extlibs/volc_core.xl`。两者以固定日期和测试凭据进行离线签名，不访问火山引擎。

## 当前验证

从 `demo6` 目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/volc.core/test/check.ps1
bash release/extlibs/volc.core/test/check.sh
```

Windows 门禁包含严格 GCC 语法检查、C 边界及离线范例。Linux 门禁
以 `-DNDEBUG`、ASan/UBSan 分离编译 C 边界与离线范例，并核对固定输出；
链接器仅移除未调用的在线 HTTP 路径，不把该路径算作已验证。
C 测试覆盖哈希/HMAC 固定向量、查询编码、签名预览、无效/不存在的日期、日期解析错误不泄漏到 XRT 线程状态、成功调用清理旧错误、查询参数第二次分配失败时的清理、大文本结果槽转移，以及双线程返回缓存、错误和状态隔离。当前发布版 `xl.exe` 在 `xhttp.body` / `xhttp.client` 依赖中首先报未知 `cstring` 类型；最小复现和主线修复建议见 `demo6/dev/extlib_blockers/volc_core_import_chain.md`。真实云请求和 XLang 调用链未通过，所以本包**尚未达到发布验收**。

2026-10-20：`available` 与 `last_status` 的 C 原型均返回 32 位 `int`，XLang 原声明分别误用 `bool` 和 64 位 `int`。现统一按 `c_int` 接入，再在公开函数边界转换为原有的 `bool`/`int`；C 定向门禁通过。上述 `xhttp.multipart` 导入阻断仍在，故尚不能宣称 XLang ABI 实跑通过。

2026-10-22：C 测试的分配故障与线程同步操作位于 `assert` 内；原测试在
`-DNDEBUG` 下会删掉实际调用。现于测试入口保持断言启用，Windows
门禁额外用发布宏严格编译该测试；预处理复核确认关键调用仍保留，
C 边界与离线范例均通过。XLang 导入链仍在 `xhttp.multipart` 的
`Buffer.appendBytes` 处失败，不据此签收双语或在线路径。

同轮横查发现四个在线/STS 入口的 `timeout_ms`、`duration_seconds` 在 C 侧也是 `int`，XLang extern 原来直接接收 64 位 `int`。现在公开函数保持原签名，传入 C 前检查 `[-2147483648, 2147483647]` 并转换为 `c_int`；超出范围抛 `RangeError`，不会静默截断。`xl parse` 通过，但完整 `xl check` 仍先被上述导入链阻断，恢复后须补 XLang 正常值、上下界和越界调用回归，并验收真实请求路径。

2026-10-22：通用签名入口拒绝上述签名字段中的控制字符，防止改变规范请求行结构或授权头；C 离线测试覆盖八个字段的 CR/LF、主机 TAB/DEL，以及含换行请求体的成功恢复。Windows 严格 GCC 和 Linux ASan/UBSan 离线门禁通过；XLang 与在线路径仍未验收。

2026-10-22：签名与请求改用同一条规范 URI；C 离线测试及范例固定验证 `/a b/中文` 的授权签名，另测 `%` 二次编码和非绝对路径拒绝。Windows 严格 GCC、Linux ASan/UBSan 离线门禁通过；真实在线 URL 仍待网络验收，XLang 导入阻断未解除。

2026-10-24：离线文本结果的 XLang 包装改为直接接管 C 结果槽，消除槽内复制之后的
第二次语言字符串复制。C 测试验证原指针转交、轮转后存活和静态文本复制；Windows
严格 C 与 Linux ASan/UBSan 离线门禁通过。XLang 导入仍被 `xhttp.multipart` 阻断。

2026-10-24：五个在线 `request*` 包装复用同一结果槽接管路径；64 KiB 原生结果
转移、接管时无额外分配、槽轮转后的存活测试通过 Windows 严格 C 与
Linux ASan/UBSan 离线门禁。
发布版 `xl.exe` 的上述 `cstring` 导入阻断仍在，未宣称在线或 XLang 运行验收。
