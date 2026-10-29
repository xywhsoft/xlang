# Socket 向量与批量收发

本增量补齐 `NetSocket` 的向量 I/O 与数据报批处理。实现驻留 runtime，复用
固定 XRT；Cgen 只生成已验证的声明、调用与拥有清理，不生成网络算法。
这是 P2 的一个增量，不代表完整网络、TLS 或五组长期任务已完成。

## 向量接口

以下 `data` 分别接受 `array<string>`、`array<bytes>`、`array<Buffer>`，
每种都是独立静态重载，不接受动态或可空元素数组：

```xlang
socket.sendVector(data, size = -1)
socket.sendVectorTo(data, target, size = -1)
socket.sendMessageVector(data, target, control, size = -1)
socket.sendMessageVector(data, control, size = -1)
socket.receiveVectorInto(output, maxBytesPerBuffer = 65536)
socket.receiveFromVectorInto(output, info, maxBytesPerBuffer = 65536)
```

所有接口返回 `(NetStatus, uint64)`，第二项是本次实际收发的**字节数**。
最后两个接口的 `output` 固定为 `array<Buffer>`；最后一个同时更新独立的
`NetDatagramInfo` 快照。显式目标入口不会隐式改用已连接目标。

- 一次至多 64 个片段。`size` 是整个向量的总字节上限，不是元素数：
  `-1` 使用全部内容，超过实际总长度时截断，低于 `-1` 报 `RangeError`。
  string 使用实际 UTF-8 字节长度，保留内嵌 NUL；前缀可以结束于编码中间。
- 输入按片段借用，不拼接为临时 string／bytes。Buffer 入口在操作期间保留
  真实原生句柄，不保留外层语言对象的地址。
- 接收在各 Buffer 的已有逻辑内容后**追加**，每个最多追加指定字节数。
  `again` 不改变逻辑内容或已有 info；预留容量可能已增长。
- 所有预留均在读取之前完成，再获取最终 Data 指针。重复的接收原生 Buffer
  在任何预留或 I/O 前报 `ValueError`，包括不同外层对象共享同一原生资源。
- 流式 socket 可以部分发送；调用者必须依据返回字节数继续发送剩余片段。
  数据报截断用 `NetStatus.truncated` 表示；空数据报不是 EOF。

## 批处理对象

```xlang
auto packet = NetDatagramPacket()             // 可选 capacity = 0
auto fromText = NetDatagramPacket("a\0b", 99) // 可选 size = -1，超长截断
auto fromBytes = NetDatagramPacket(b"abc")
auto shared = NetDatagramPacket(Buffer())
auto storage = packet.shareBuffer()
auto progress = NetBatchProgress()
```

Packet 持有一个真实 Buffer 原生拥有边；文本／bytes 构造复制输入，Buffer
构造共享其原生存储。`shareBuffer()` 创建新的语言 Buffer 外壳，仍共享同一
原生存储；它不是原外壳的引用身份。外壳被销毁后，另一拥有者仍可继续使用。
共享 Buffer 不提供额外并发同步，调用者不得同时修改或参与另一次 I/O。

Packet 的公开能力：

| 接口 | 合同 |
|---|---|
| `setTarget(target)`／`clearTarget()`／`hasTarget` | 复制目标值；无目标时用于已连接发送 |
| `target` | 返回独立地址副本；未设置目标时 checked 报错 |
| `hasPacket` | 是否已有接收包信息；构造及 clear 后为 false |
| `status` | 最近一次该元素的接收结果；尚无结果时 checked 报错 |
| `info` | 返回独立接收快照；尚无包信息时 checked 报错 |
| `clear()` | 清空共享 Buffer 的逻辑内容与接收结果，保留容量和目标 |

## 批处理接口

```xlang
socket.sendBatch(packets, progress, start = 0, count = -1)
socket.receiveBatch(packets, progress, maxBytesPerPacket = 65536, start = 0, count = -1)
```

`packets` 为 `array<NetDatagramPacket>`，`progress` 必须显式提供。返回
`(NetStatus, uint64)`，第二项是本次完成的**数据报数**，不是字节数。

- `start` 允许 0 到数组长度；`count = -1` 使用剩余全部，超出剩余长度则
  截断；其余负数报错。选择的片段最多 64 个，空选择合法。
- 发送使用各 Packet 的完整 Buffer 内容与可选目标，不进行字节拼接。
- 接收**替换**已消费 Packet 的逻辑内容、状态及 info，不追加；未消费的
  后缀完整保留。单包截断体现在该 Packet.status，不等同整批失败。
- 重复接收原生 Buffer 在预留和 I/O 前拒绝；全部预留在消费数据报前完成。
  失败或 `again` 前预留的容量可能增长，但未消费元素的逻辑内容不变。
- `progress.clear()` 重置为未尝试、failed、0／0。方法在工作前清空旧进度；
  参数校验或预留失败时 `attempted` 为 false。进入原生批处理后为 true，
  `status`／`completedCount`／`startIndex` 在传播 checked 错误前保存。
  因而批处理中途失败时仍可检查已完成前缀，不能盲目重发整个批次。
- 错误边界保留首个操作失败；清理错误不覆盖已记录进度。底层拥有图显式
  包含 Packet → Buffer 边，不依赖生成对象布局猜测或临时回调视图。

## 验证范围

入口为 `tools/check_stdlib_net_vector_runtime.py`（独立原生闭包）与
`tools/check_stdlib_net_vector_source.py`（实际源码编译器）。后者覆盖静态
负例、XLIR 合同篡改拒绝、严格 C O0／O2、JIT、模块、独立 AOT、导入与裁剪，
以及完整语言对象分配失败前缀。仅使用自身 IPv4／IPv6 loopback 临时端口和
离线生命周期／参数检查，不进行外部网络、多播流量或网络安全性质测试。

源码验收不等于已替换 release／VFS 产品；旧发布产品与后续 P7 联合验收
保持分开记录。
