# ftpserver — FTP server (command + PASV data channel)

用 xlang 原生 `tcp.listen` / `tcp.connect` / `TcpServer` / `TcpStream` 实现的**真实可用**
RFC 959 子集 FTP 服务器，压测 TCP 命令/数据双连接、FTP 协议状态机、文件 I/O。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\ftpserver\ftpserver.xl                 :: self-test
xl run projects\dev\ftpserver\ftpserver.xl test             :: self-test
xl run projects\dev\ftpserver\ftpserver.xl serve 12121 ftproot   :: 常驻
xl build projects\dev\ftpserver\ftpserver.xl -o projects\dev\ftpserver\ftpserver.exe
```

- `ftpserver` / `ftpserver test`：自启动 server（`tcp.listen(0)` 自动选端口）+ 工作线程模拟
  客户端跑完整协议，47 项断言全过后退出码 0。
- `ftpserver serve <port> [rootdir]`：常驻监听 `127.0.0.1:<port>`，rootdir 缺省为 `ftproot`。
  支持并发客户端（一次接受一个，串行处理）。
- `ftpserver -h | --help`：用法。

## 支持命令

`USER PASS QUIT PWD CWD CDUP LIST RETR STOR DELE MKD RMD TYPE SYST NOOP PASV`

- 认证：`anonymous` / `ftp`（任意密码）或 `admin` / `admin`；未登录前除 USER/PASS/QUIT 外一律 `530`。
- PASV：被动模式，server 开新 TCP 端口传数据。回复 `227 Entering Passive Mode (127,0,0,1,p1,p2).`。
- 虚拟路径 + 根目录限制：`..` 在虚拟路径层 clamp 到根，并再做一次 `path.normalize` 前缀校验，
  `CWD/RETR/STOR/DELE/MKD/RMD` 都无法越出 rootdir。
- LIST 输出 `ls -l` 风格文本；RETR/STOR 走 BINARY（`file.readBytes` / `file.writeBytes`）。

## 架构要点（与 xlang 限制的博弈）

- **单一 session 数据 listener**：每个客户端连接建立时 `tcp.listen(0)` 创建一个 PASV 数据
  监听器，整个会话复用。PASV 只回端口；真正的 `accept` 延迟到 LIST/RETR/STOR 到达时才做
  （此时客户端已连上）。这样既避免对同一 point 重复包装 TcpServer（见 BUG-FTP3，会堆破坏），
  也避免在 PASV handler 里阻塞 accept 把命令通道饿死。
- **point handle 跨函数、TcpStream 包装一次**：`handleClient(point, string)` 收 point，
  内部构造唯一一个命令通道 TcpStream；每个 accept/connect 出来的 point 全程只包一次。
  TcpStream 作为参数传给 `sendLine`/`recvLine`/`sendBytesRetry` 是安全的（BUG-HD1）。
- **行缓冲**：`recvLine` 在持久 `inbuf` 上找 `\r\n` 切一行，剩余留给下次，跨 `recv` 分片正确。
- **自检线程**：`test` 模式 server accept 在 main，客户端跑在工作线程；全局只用标量
  （gTestPort/gPassed/gFailed，避开 BUG-XT3 的容器重绑定坑）。

## 发现并记录的 BUG（详见 `../BUGS.md`）

| BUG | 现象 | 绕过 |
|---|---|---|
| BUG-FTP1 | accept 后服务端**首次** send/sendBytes 返回 false 且对端收不到 | 发送重试循环（5ms × 30） |
| BUG-FTP2 | `close()` 是 abortive，丢弃未 ACK 的发送缓冲；`drain` 不等对端 ACK | close 前 `sleep(300ms)` 让内核投递 |
| BUG-FTP3 | 同一 point 二次 `TcpStream(p)` 触发堆破坏 `0xC0000374` | 每个 point 只包装一次 |

## 验证

- `xl run` self-test：47/47 passed，EXIT=0。
- `xl build` 产出 exe，再运行：47/47 passed，EXIT=0。
- 跨进程验证：`serve` 模式常驻，外部 `xl run` 客户端连入（USER/PASS/PWD/TYPE/PASV/RETR/QUIT），
  10/10 passed，RETR 真实文件内容一致。
