# chatroom - HTTP 聊天室

用 `http.server` 实现的多用户聊天室，验证 **HTTP server 并发状态管理** 与 **长轮询（long-polling）**。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\chatroom\chatroom.xl
xl build projects\dev\chatroom\chatroom.xl -o projects\dev\chatroom\chatroom.exe
chatroom                 :: self-test（启动 server、自发自收 44 项用例、退出）
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

## 并发状态设计

- `array<int>` 保存单调消息 ID，`array<string>` 保存一次编码的不可变 JSON 记录；`var gSessions` 保存 token 到用户名的字典。
- `mutex gLock` 保护消息、会话和自增编号；锁内只做短小的内存读写。
- 查询消息时在锁内复制匹配的不可变字符串，在锁外用 `string.join` 组装 JSON 数组，避免重复解析和序列化。
- 长轮询只在检查最新消息编号时短暂持锁，等待期间不占锁，因此发送请求可以唤醒轮询。
- 服务使用 4 个 worker；自测分别验证 POST 路由和异步长轮询状态唤醒，避免把两个故障边界混在同一断言中。
- 顺序 HTTP 用例复用一个 `HttpClient` 连接池，长轮询使用独立异步客户端。

## 正向结论

- `mutex` 保护消息日志、会话和编号的原子更新；跨线程消息记录只以不可变字符串发布。
- 长轮询通过 `executeAsync()` 和 `future.wait()` 完成，不用普通 OS 线程包装阻塞式 HTTP 请求。
- 新消息发布后 poll 会在下一次检查周期返回，明显早于 5 秒 timeout。

## 注意

响应 JSON 使用字典和 `json.stringify` 构造，用户名、消息和查询内容中的引号或控制字符不会破坏 JSON。
