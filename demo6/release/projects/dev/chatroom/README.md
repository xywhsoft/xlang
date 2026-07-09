# chatroom - HTTP 聊天室

用 `http.server` 实现的多用户聊天室，验证 **HTTP server 并发状态管理** 与 **长轮询（long-polling）**。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\chatroom\chatroom.xl
xl build projects\dev\chatroom\chatroom.xl -o projects\dev\chatroom\chatroom.exe
chatroom                 :: self-test（启动 server、自发自收 42 项用例、退出）
chatroom serve [port]    :: 常驻模式（默认 18300），浏览器打开 http://127.0.0.1:18300
```

## 路由

| 方法 | 路径 | 说明 |
|---|---|---|
| GET  | `/` | 聊天页面（内嵌 HTML + JS 客户端） |
| POST | `/api/login` | 登录，body `{username}`，下发 `chat_token` cookie |
| POST | `/api/message` | 发送消息，body `{text}`（需 cookie 鉴权） |
| GET  | `/api/messages?since=N` | 消息列表（增量，id > N） |
| GET  | `/api/poll?since=N&timeout=M` | 长轮询，最多等 30s，有新消息立即返回 |
| GET  | `/api/users` | 在线用户列表 |
| POST | `/api/logout` | 登出，清除会话与 cookie |

## 并发状态设计（绕过 BUG-XB1）

`dev/BUGS.md` **BUG-XB1**：文件作用域 `var` 容器在 HTTP 回调里**元素引用丢失**（length 对、元素空）。
本工具的状态全部用**全局 string + int** 承载（这两种在回调里访问完全正常），每次请求把 string
`json.parse` 成**局部 var** 再读写（局部 var 在回调里正常）：

- `string gMessages` — JSON 数组 `[{id,user,text,time}]`
- `string gSessions` — JSON 对象 `{token: username}`
- `int gNextId` / `int gNextTokenSeq` — id / token 自增计数器
- `mutex gLock` — 串行化 parse-modify-stringify 临界区（workerCount=4，多 worker 下必需）

**长轮询**：`/api/poll` 里 sleep + 检查 `gNextId-1 > since` 的循环，**循环里不持锁**（只持锁读一个 int），
否则 POST 拿不到锁会死锁；有新消息或超时后，一次性 parse + 过滤返回。

## 正向结论

- `mutex` 全局对象在 HTTP C-callback 里 `lock/unlock` **完全正常**（BUG-XB1 只影响 var 容器元素，
  不影响 native-backed class 全局），多 worker 下共享可变状态可安全串行化。
- 长轮询在多 worker（workerCount=4）下工作：后台线程 poll 期间，另一 worker 处理 POST，
  poll 被新消息唤醒（self-test 实测 ~0.8s 唤醒，远小于 5s timeout）。

## 注意

- 局部变量**不能命名为 `id`**：与 stdlib `id` 命名空间冲突（`id.toString()` 被解析成命名空间访问，
  报 `standard library namespace has no such symbol`）。改用 `msgId` 等名字即可。
- `for x in c ... end` **不带 `do`**（只有 `while` 要 `do`）。
