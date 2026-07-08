# prodcon - producer/consumer concurrency demo (channel + thread)

通过多个 producer thread 向一个共享 `channel<int>` 投递整数、多个 consumer thread 从同一 channel 消费，验证 xlang 的 `channel<T>` + `thread` 并发原语。

## 用法

```
xl run projects\dev\prodcon\prodcon.xl -- run [producers=3] [consumers=2] [items=10]
```

或先 build 再运行：

```
xl build projects\dev\prodcon\prodcon.xl -o projects\dev\prodcon\prodcon.exe
projects\dev\prodcon\prodcon.exe run [producers=3] [consumers=2] [items=10]
```

参数：

| 参数 | 默认 | 说明 |
|---|---|---|
| producers | 3 | producer 线程数（必须 > 0） |
| consumers | 2 | consumer 线程数（必须 > 0） |
| items | 10 | 总共要生产的整数项数（必须 > 0） |

channel 使用固定容量 4 的有界缓冲。每个生产/消费事件实时打印，末尾汇总 `sent total` / `received total`，并给出 `RESULT: OK`（全部投递成功，exit 0）或 `RESULT: MISMATCH`（exit 1）。

## 设计要点（绕过现有 BUG 的关键决策）

1. **`thread` 不可复制，不能进 `array<thread>`/`list<thread>`**（语义阶段报 `typed container item type must be copyable`）。因此 producer/consumer 采用**fire-and-forget** 方式启动，线程数可变而不需要容器存储；协调改用 main 在启动前预先置入全局 `g_activeProducers`/`g_activeConsumers` 计数，各线程退出时在 mutex 下递减，main 轮询到 0 推进下一阶段（规避启动竞态）。
2. **全局 `mutex` 必须 `= mutex()` 显式初始化**（见 BUG-31）。裸声明 `mutex g_lock` 是一个空操作互斥——`lock()`/`unlock()` 返回 true 但**完全不串行化**，导致临界区丢更新。
3. **consumer 不使用阻塞 `recvInto`，改用 `tryRecvInto` 轮询**（见 BUG-32）：阻塞在 `recvInto` 的线程在 channel 被 `close()` 后**不会被唤醒**，会永久挂起。consumer 在 `tryRecvInto` 失败后检查 `isClosed() and count() == 0` 退出。
4. **producer 的 `send` 必须重试**：`channel.send` 是**非阻塞 try-enqueue**（缓冲满或 channel 关闭时返回 false，见 NOTE-32 / `examples/concurrency.xl:803-806`），不是阻塞投递。producer 在 `send` 返回 false 时短睡重试，直到成功或 channel 被关闭。
5. item id 由单一全局计数器 `g_nextItem` 在 mutex 保护下分发，保证总产量恒等于 `items`，与 producer 数量无关。

## 发现的 BUG

- **BUG-31**：裸全局 `mutex`（未 `= mutex()` 初始化）是静默空操作互斥，`lock/unlock` 不串行化。
- **BUG-32**：阻塞在 `channel.recvInto()` 的线程在 channel `close()` 后不被唤醒，永久挂起。

详见 `../BUGS.md`。

## 验证

- `xl run` 与 `xl build`+exe 双路径均通过。
- 配置矩阵（producers, consumers, items）：默认 3/2/10、5/5/20、1/1/1、1/3/15、4/1/12、4/4/50 全部 `RESULT: OK`。
- 默认配置连跑 15 次，15/15 `RESULT: OK`，无间歇性竞态。
- 错误路径：无参 / 未知命令 / producers≤0 / 非数字 producers 均正确报错并 exit 1；正常 run exit 0。
