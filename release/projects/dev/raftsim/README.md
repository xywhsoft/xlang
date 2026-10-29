# raftsim - Raft 共识算法模拟器（5 节点集群）

用 xlang 的 async 定时器 + 状态机 + 多节点一致性 + channel 跨节点通信，模拟一个
5 节点 Raft 集群的完整生命周期：Leader 选举、日志复制、网络分区、恢复、重新选举。

## 运行

```bat
cd /d D:\GIT\x-lang\release
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

## RPC 传输（channel<string> + 类型化解码）

每个节点拥有独立 `channel<string>` inbox。发送端把只包含整数的 RPC 编码为紧凑
逗号分隔信封，接收端立即解码成局部 `Rpc` class：

```
sender:                                    receiver:
  Rpc rpc = Rpc()                            inbox.tryRecvInto(wire)
  rpc.term = term                            Rpc rpc = decodeRpc(wire)
  inbox[dst].send(encodeRpc(rpc))             processRpc(id, rpc)
```

- 消息是 channel 拥有的 string 副本，不共享发送端对象。
- 解码后的 `Rpc` 只属于接收线程，不需要额外锁保护消息字段。
- 删除了 slot 分配器、RPC 全局锁和 13 张并行数组，字段不会发生错位。

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

## 并发边界

| 边界 | 处理方式 |
|---|---|
| channel / thread 是不可复制的 native-backed 资源 | inbox 用 5 个明确命名的全局 channel；节点线程由闭包工厂捕获 id |
| `channel.send` 是有界队列投递 | inbox 容量 128；节点每轮先排空 inbox，再执行定时状态机 |
| 节点状态由多个线程读取 | `g_lock` 保护状态快照和状态迁移；发送消息时不持锁 |
| 定时选举需要单调时间 | 使用 `time.timer()` 计算 deadline，`time.sleep(ms)` 控制轮询节奏 |

## 设计要点

- **粗粒度全局锁 `g_lock`**：保护所有共享状态。唯一硬规则：**持锁期间绝不
  `channel.send`**（避免 receiver 也需要锁时死锁）。每个 RPC handler 先持锁做状态
  计算、释放锁，再发送响应。
- **单条目 AppendEntries**：每条 AppendEntries 至多携带 1 个 entry（足够演示复制与
  追赶）。追赶靠 nextIndex 回退 + 逐条重试。
- **commitIndex 只提交本任期 entry**：遵循 Raft 5.4.2（leader-commit rule），
  `advanceCommit` 只对 `logTerm[N] == currentTerm` 的 N 计数多数派。
