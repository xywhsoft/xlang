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
cd /d D:\GIT\x-lang\release
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

## 运行时边界

- 内置测试服务器用 `sendRetry` 处理有界发送队列暂时繁忙的情况，重试有次数和间隔上限。
- QUIT/221 完成后短暂等待发送队列排空，再关闭连接，避免截断最后一条回复。
- 每个 native handle 只构造一个 `TcpStream` owner，并按 class 默认引用语义传给辅助函数。
- 测试附件使用随机临时文件名，结束后删除，允许多个测试进程并行运行。
- `time.timezoneOffset()` 返回秒，Date 头通过秒数计算 `+HHMM` / `-HHMM`。
