# 聚合 Server 的拥有型 Listener 前置

2026-10-21：已完成 XRT 原生前置，不代表聚合 TcpServer 的语言接入完成。
XRT 独立提交：`2ecf0443af254c6913734eb95dbd5220bb6850db`。

## 本批实际交付

新增 `xrtNetListenOwnedV1` 与 `xrtNetListenerOwnershipAdapterV2`：

- Listener 创建成功转移一份真实事件上下文引用，失败不消费。
  与原有创建／接受算法共用实现，不增加上下文堆分配。
- Accept 可在目标 Worker 并发执行；Error／Close／Drop 在 Listener Worker。
  接受的 Stream 不继承上下文或裸 Stream 回调。
- Close 后使用嵌入式、无分配的命令延迟 Drop，保留真实 runtime／Engine 边直到
  回调任务尾部退出；Drop 仍在执行时只能报告 BUSY，不能以 CLOSED 代替退休。
- V2 按独立提供的精确策略身份准入后才读取元数据。未知地址、伪造副本和
  旧裸回调继续拒绝；失败不修改输出或当前诊断。
- Trace 增加一条真实 Data 引用边；不把 Child 内部地址、命令或借用别名算作 RC。
  Prepare 复用实际关闭和 Worker 排空，不通过丢弃回调或清零计数破环。
- Listener Data 改为原子快照；拥有上下文退役后返回空。

## 验证证据

证据根：`F:/xlang-validation/five-groups-20261021/p3-listener-events`。

| 最终报告 | 阶段 |
|---|---:|
| native-win-r2/results.json | 306 |
| native-linux-r2/results.json | 381 |
| server-compat-win-r1/results.json | 16 |
| server-compat-linux-r1/results.json | 20 |
| 合计 | 723 |

覆盖 GCC／TCC O0／O2、模块化／单头，Linux 增加 ASan／UBSan；输入、日志和
产品哈希均不变。新夹具每次运行 6 个生命周期场景、31 次策略拒绝、5 个创建
分配预算（4 个失败前缀），并检查完整拥有图、跨 Worker 重入关闭、真实回调尾部、
阻塞 Drop 的 BUSY、checked Prepare 和精确内存平衡。旧 TCP／Engine／缓冲及
原始聚合 Server 普通接受、Future 取消／关闭路径一并回归。

初轮 TCC 夹具聚合初始化触发编译器内部错误，改为等价逐项赋值后整轮复测。
发现记录保留，但不计入上述最终通过；没有放宽产品合同。
全部网络通信仅为自建字面量回环正常流量；没有 DNS、公网、协议探测、畸形数据
或网络安全性质测试。

## 后续仍需完成

1. 原生聚合 Server 自身的 Engine／Listener／接受队列拥有图、Child 回调绑定和
   checked 退休。当前 Child 只是内部稳定数组地址，没有可假定的独立 RC。
2. Server Accept Future 的真实生产者、取消 Watch、拥有型 Stream 结果和终态清理；
   不能把旧 raw Promise／Watch 路径当作已有完整图合同。
3. 不可变多端点配置、聚合状态／统计、同步／异步接受和生命周期的驻留语言入口。
4. 固定快照的选择性同步及实际 JIT／独立 AOT／裁剪验收。

本批尚未同步到 xlang 的固定 XRT／xruntime 头；现有产品 ABI、微秒／绝对期限
合同和发布二进制均未修改。后续随 Server 原生闭包统一同步并验收，避免将
独立 XRT 仓库的新毫秒 ABI 或无关文件／内存改动带入固定产品。

引用／接管发送、完整 UDP／多播、网络文件／任务组合、P0／P1 剩余项及 P4—P7
继续开放。未决策仍为 CRT 0／XRT 11，本批没有扩大或缩减批准范围。
