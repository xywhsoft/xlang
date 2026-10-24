# jwt

JWT 的 HS256 紧凑格式基础能力，签名使用 XRT HMAC-SHA256，Base64URL 和
JSON 解析也复用 XRT。XLang 使用 `import jwt`；C 使用
`include/jwt_native.h` 并链接 `jwt_native.c` 与 XRT。
可运行的对照范例见 `release/examples/extlibs/jwt.xl` 和 `examples/basic.c`。

| API | 含义 |
| --- | --- |
| `encode(payload_json, secret)` | 对 JSON 对象 claims 签名，返回紧凑 token；失败返回空字符串。 |
| `verify(token, secret)` | 仅验证 HS256 签名、算法和 claims JSON 对象形状。 |
| `decode(token)` | 读取未经认证的 payload，**不能**据此授权。 |
| `header(token)` | 读取未经认证的 header，**不能**据此选择算法或密钥。 |

`verify` **不检查** `exp`、`nbf`、`iss`、`aud` 等业务声明，也不做密钥轮换。
需要认证决策的程序必须在验签成功后自行检查这些声明；此包当前不是完整的
身份认证中间件。秘钥不得为空，生产环境应使用足够强的随机密钥。编码与检查
结果中的空字符串或 `false` 表示失败；C 可用 `xjwt_error()` 读取当前线程的
最近错误。XLang 参数包含内嵌 NUL 时抛 `ValueError`，避免 C 字符串静默截断。

C 的 `encode/decode/header` 返回调用方拥有的 UTF-8 文本；失败返回 `NULL`，
成功结果均须用 `xrtFree` 释放。各结果相互独立，释放 token 不影响已读出的
payload/header。最近错误仍存于 XRT 线程键，线程退出时自动释放。
XLang 公开结果为 `cstring`，直接借用这份 C 分配并在最后一个别名释放时调用
`xrtFree`；赋给普通局部 `string` 也保持同一拥有链。进入 `var`、容器或 C
公开导出适配器时，按语言规则物化为普通字符串。

在 `demo6` 目录执行 `release/extlibs/jwt/test/check.ps1`。脚本覆盖严格 GCC、
C 单测和双语范例、XLang 原用例与负例、AOT 对照；开发时 `-SkipAot` 可
只跑定向路径。所有编译产物写在独占的 `build/test/jwt_guard_*` 目录并清理。
Linux 上可执行 `bash release/extlibs/jwt/test/check.sh`，使用 ASan/UBSan
验证同一 C 用例。2026-10-25 Windows 完整门禁与 Linux C sanitizer 均通过；
Linux 的旧 `release/xl` 尚未重建，不计入本轮语言侧签收。
