# volc.core

火山引擎签名与请求的共享基础包。离线能力包括 SHA-256、HMAC-SHA256、规范化查询、JSON 字符串编码和签名预览；在线请求依赖 `xhttp.client`。`volc.phone` 与 `volc.sts` 复用本包。

```xlang
import volc.core

string query = volc.core.canonical_query("ListApps", "2020-01-01", "Limit=10\nName=x lang")
string preview = volc.core.sign_preview_acep(
    "ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240102T030405Z")
```

`extra_pairs` 是逐行 `key=value` 文本，规范化时按键和值排序并对 UTF-8 字节做百分号编码。`x_date` 为空时使用 XRT 当前 UTC 时间；显式指定时必须是有效的 `YYYYMMDDTHHMMSSZ`，无效日期直接报错，不静默改用当前时间。预览返回含 `query`、`x_date`、`x_content_sha256`、`signed_headers`、`authorization` 的 JSON 字符串，不发网络请求。预览中的凭据只应使用测试值；真实授权头不应写入日志。

## C 接口

声明见 `include/volc_core_native.h`。返回的 `const char*` 由包持有，调用方不得释放；需要跨线程或长期保存时应立即复制。返回槽及 `last_error`/`last_status` 使用 XRT 原生线程局部键，线程退出时清理；不同线程的诊断与返回缓存不会互相覆盖。**这只是离线签名层的线程隔离验证，不是在线客户端的并发认证。**在线 `request*` 会创建并销毁一个 XRT 网络引擎及 HTTP 客户端，尚未验收连接复用、并发和真实服务行为。

原生 C 范例在 `examples/signing.c`；XLang 范例在 `release/examples/extlibs/volc_core.xl`。两者以固定日期和测试凭据进行离线签名，不访问火山引擎。

## 当前验证

从 `demo6` 目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/volc.core/test/check.ps1
```

门禁包含严格 GCC 语法检查、C 边界及离线范例。C 测试覆盖哈希/HMAC 固定向量、查询编码、签名预览、无效/不存在的日期、日期解析错误不泄漏到 XRT 线程状态、成功调用清理旧错误、查询参数第二次分配失败时的清理，以及双线程返回缓存、错误和状态隔离。XLang 范例当前在导入依赖 `xhttp.multipart` 时失败，最小复现和主线修复建议见 `demo6/dev/extlib_blockers/volc_core_import_chain.md`；本任务不修改 XRT 专门维护的包。真实云请求和 XLang 调用链未通过，所以本包**尚未达到发布验收**。

2026-10-20：`available` 与 `last_status` 的 C 原型均返回 32 位 `int`，XLang 原声明分别误用 `bool` 和 64 位 `int`。现统一按 `c_int` 接入，再在公开函数边界转换为原有的 `bool`/`int`；C 定向门禁通过。上述 `xhttp.multipart` 导入阻断仍在，故尚不能宣称 XLang ABI 实跑通过。

2026-10-22：C 测试的分配故障与线程同步操作位于 `assert` 内；原测试在
`-DNDEBUG` 下会删掉实际调用。现于测试入口保持断言启用，Windows
门禁额外用发布宏严格编译该测试；预处理复核确认关键调用仍保留，
C 边界与离线范例均通过。XLang 导入链仍在 `xhttp.multipart` 的
`Buffer.appendBytes` 处失败，不据此签收双语或在线路径。

同轮横查发现四个在线/STS 入口的 `timeout_ms`、`duration_seconds` 在 C 侧也是 `int`，XLang extern 原来直接接收 64 位 `int`。现在公开函数保持原签名，传入 C 前检查 `[-2147483648, 2147483647]` 并转换为 `c_int`；超出范围抛 `RangeError`，不会静默截断。`xl parse` 通过，但完整 `xl check` 仍先被上述导入链阻断，恢复后须补 XLang 正常值、上下界和越界调用回归，并验收真实请求路径。
