# smtpcli - SMTP 客户端（发送邮件）

用裸 TCP 实现 SMTP（RFC 5321）发送客户端，验证 TCP 协议状态机、Base64、
string 处理、MIME 格式构造。

## 用法

```
smtpcli send --host <host> --port <port> --from <addr> --to <addr>
             --subject <text> --body <text>
             [--user <user> --pass <pass>]        AUTH LOGIN（Base64）
             [--cc <addr>]                        可重复
             [--attach <file>]                    可重复，Base64 附件
             [--heloname <name>]                  默认 localhost
             [--timeout <ms>]                     默认 15000
smtpcli test                                   self-test（本地 server + 客户端往返）
smtpcli -h | --help
```

## 验证

```
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\smtpcli\smtpcli.xl -- test
xl build projects\dev\smtpcli\smtpcli.xl -o projects\dev\smtpcli\smtpcli.exe
```

- `xl run ... -- test` 与 `xl build` 后 `smtpcli.exe test`：self-test 32/32 全过，EXIT=0。
- 真机验证：另起一个 SMTP 调试服务（Python socket 实现），`smtpcli send` 走完整
  EHLO→(AUTH LOGIN)→MAIL FROM→RCPT TO→DATA→QUIT 链路，多收件人 / Cc / 附件 /
  Unicode Subject / dot-stuffing 全部正确，服务端实测收到合规 MIME。

## 实现要点

- **协议交互**：`recvReply` 处理 SMTP 多行回复（`250-` 续行 / `250 ` 终止行）；
  `recvDataTerminator` 读 DATA 直到 `<CRLF>.<CRLF>`。
- **AUTH LOGIN**：`codec.base64Encode` 编码用户名/口令；挑战串
  `VXNlcm5hbWU6`/`UGFzc3dvcmQ6`（= base64("Username:")/base64("Password:")）。
- **MIME 构造**：From/To/Cc/Subject/Date/MIME-Version/Content-Type；非 ASCII 主题走
  RFC 2047 encoded-word（`=?UTF-8?B?...?=`）；附件用 `multipart/mixed` +
  Base64（76 列折行）。
- **Dot-stuffing**：DATA payload 中以 `.` 开头的行前补 `.`，保证 SMTP 透明性。
- **Date 头**：RFC 2822 格式 `Thu, 10 Jul 2026 12:00:00 +0800`，用 `time.*` 部件手拼。

## 设计约束绕过

见 `dev/README.md`「当前限制」与 `dev/BUGS.md`：

- **BUG-FTP1**（accept 后服务端首次 send 丢失）：内置 test server 的所有回复走
  `sendRetry` 重试循环（~5ms 间隔、最多 40 次）。
- **BUG-FTP2**（`close()` abortive 丢未 ACK 缓冲）：QUIT/221 交换后
  `time.sleep(QUIT_CLOSE_DELAY)` 再 close。
- **BUG-FTP3**（同 point 二次包装 TcpStream 堆破坏）：每个 accept/connect point
  只 `TcpStream(p)` 一次，全生命周期复用；`TcpStream` 仅作函数参数按值传入（安全）。
- **BUG-CE1**（`var + string` 乱码）：所有拼接用 `string` 类型变量 + `string(v)` 强转。
- **BUG-RT4**（class 构造嵌套需 statement-level lowering）：不内联 `Buffer()`，
  先声明再传入。
- **native-backed class 不可被闭包按值捕获**：test server 的 `TcpServer` 留在 main
  中 accept，仅 client（捕获一个 `int port`）跑在 worker thread（ftpserver 模式）。
- **BUG-SM1**（新发现）：`time.timezoneOffset()` 返回秒而非文档所称分钟，
  按 `/3600` 取小时绕过。
