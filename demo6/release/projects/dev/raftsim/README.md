# raftsim - Raft 共识算法模拟器（5 节点集群）

用 xlang 的 async 定时器 + 状态机 + 多节点一致性 + channel 跨节点通信，模拟一个
5 节点 Raft 集群的完整生命周期：Leader 选举、日志复制、网络分区、恢复、重新选举。

## 运行

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\raftsim\raftsim.xl -- test
xl build projects\dev\raftsim\raftsim.xl -o projects\dev\raftsim\raftsim.exe
raftsim.exe test
```

## 命令

```
raftsim test    完整模拟：初始选举 → 日志复制 → 分区 → 恢复 → 重新选举
```

## 架构

```
5 nodes (id 0..4), each runs in its own thread:

  nodeLoop(id):
    loop while g_running:
      1. drain inbox (tryRecvInto) -> processRpc per RPC
      2. periodic:
           Leader    -> leaderHeartbeat every 60ms (AppendEntries)
           Follower  -> election timeout (200-450ms randomized) -> startElection
           Candidate -> election timeout (200-450ms randomized) -> new election
      3. sleep(2ms)
```

节点状态机：`Follower → Candidate → Leader（或回退 Follower）`，严格遵循 Raft 论文
Figure 2 的四类规则：

- **RequestVote**：任期比较、日志新旧比较、投票授予、重置选举定时器。
- **RequestVoteResponse**：得票过半 → 成为 Leader；发现更高任期 → 降级。
- **AppendEntries**：心跳 + 日志追加；prevLogIndex/prevTerm 一致性检查；commitIndex 推进。
- **AppendEntriesResponse**：成功 → 更新 matchIndex 并尝试提交；失败 → nextIndex 回退重试。

## RPC 传输（channel<int> slot 信封）

`channel<T>` 只接受 int/bool/float 等值类型（**BUG-AC1**），不能传字符串或结构体。
因此采用 **slot 信封**：

```
sender:                                    receiver:
  rpcAlloc() -> slot s                       inbox.tryRecvInto(slot) -> s
  g_rpcType[s], g_rpcTerm[s], ... = payload  读 g_rpcType[s], g_rpcTerm[s], ...
  inbox[dst].send(s)                         processRpc(id, s)
```

- 13 个并行全局数组（`g_rpcType`/`g_rpcTerm`/`g_rpcPrevIdx`/...）按 slot id 索引，
  承载一条 RPC 的全部字段。
- `rpcAlloc` 在 mutex 下 push 一整行，保证 slot id 在所有数组中同步可用。
- channel 的 send→recv 提供 happens-before，receiver 读到的 slot 字段一定完整。

## 网络分区

链路矩阵 `g_link[a*NODES+b]`：`LINK_ON` 可达，`LINK_OFF` 断开。所有 send 函数先
`canSend(src,dst)` 检查链路，分区时消息直接丢弃（不投递）。

- `partitionNode(n)`：断开 n 与所有其它节点（双向）。
- `healAll()`：恢复全连通。

分区语义验证：
- 隔离旧 Leader（少数派）→ 其 AppendEntries 投递不到多数派；多数派侧超时重新选举。
- 多数派侧选出新高任期 Leader，继续提交日志；旧 Leader 提交停滞。
- 恢复后旧 Leader 的旧任期 AppendEntries 被多数派拒绝（返回更高任期）→ 旧 Leader
  发现更高任期 → 降级 Follower；新 Leader 心跳把旧 Leader 日志追平。

## 测试覆盖（21 个断言）

| 阶段 | 断言 |
|---|---|
| 初始选举 | 唯一 Leader 选出；无竞争 Leader；任期推进 |
| 日志复制 | 3 条 entry 全部被 Leader 接收；复制到多数派；复制到全部 5 节点；commitIndex=3 |
| 分区 | 隔离前 Leader 存在 |
| 重新选举 | 多数派侧选出新 Leader；新 Leader ≠ 旧 Leader |
| 分区下复制 | entry 4 被新 Leader 接收；多数派复制到 entry 4；旧 Leader 未收到 entry 4 |
| 恢复 | 旧 Leader 降级为 Follower；仍有 Leader；回到单一 Leader（无脑裂） |
| 一致性恢复 | entry 5 被接收；全部 5 节点复制到 entry 5；全部节点日志 term 完全一致 |

## xlang 限制绕过

| 限制 | 绕过 |
|---|---|
| **BUG-AC1**：`channel<string>` 不支持 | 全部 RPC 用 `channel<int>` + slot 信封（并行全局数组承载字段） |
| channel / thread 不可复制，不能进 typed 数组 | inbox 用 5 个预声明全局 channel；node body 用闭包工厂 `makeNodeBody(id)` 捕获 id（按值），fire-and-forget `thread(...)` |
| `channel.send` 非阻塞 try-enqueue；close 后 `recvInto` 永挂（BUG-32） | 节点用 `tryRecvInto` 轮询，从不 close inbox；inbox 容量 128 远大于峰值 |
| **BUG-XT3**：main 中重绑定全局容器致线程崩溃 | 所有全局数组在声明处 `array[]` 初始化，仅 push / index 赋值就地修改，从不 `g = [...]` 重绑定 |
| `time.clock()` 在 `xl run` 下恒 0（BUG-XL7） | 计时用 `time.timer()`（float 秒，单调墙钟，`xl run` 下正常） |
| 复合赋值不派发 class 协议（BUG-V1） | 本工具不使用 class 运算符协议；int 的 `+=` 正常 |

## 设计要点

- **粗粒度全局锁 `g_lock`**：保护所有共享状态。唯一硬规则：**持锁期间绝不
  `channel.send`**（避免 receiver 也需要锁时死锁）。每个 RPC handler 先持锁做状态
  计算、释放锁，再发送响应。
- **单条目 AppendEntries**：每条 AppendEntries 至多携带 1 个 entry（足够演示复制与
  追赶）。追赶靠 nextIndex 回退 + 逐条重试。
- **commitIndex 只提交本任期 entry**：遵循 Raft 5.4.2（leader-commit rule），
  `advanceCommit` 只对 `logTerm[N] == currentTerm` 的 N 计数多数派。
