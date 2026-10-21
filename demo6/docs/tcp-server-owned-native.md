# 聚合 Server／Accept Future 原生闭包验收

2026-10-21，XRT 提交 `c09e357a7381d052f26d2ae28220c0ffa5ac9ba0`。
这是语言聚合 Server 的原生前置，不是公开标准库／JIT／AOT 或 shipping 完成。

## 实际实现

- Server 对 Engine 有真实引用和登记的 runtime 槽，不再只有裸 Engine 指针和
  难以解释的 RuntimeHeld 自引用。创建者、队列、Future 和绑定各自按实际引用计数。
- 内嵌 Child 有实际绑定计数和一份父 Server 引用，内嵌地址本身不被当作 RC 根。
  Clear 保留父引用至最后一次绑定 Drop，避免计划仍 pin 着 Child 时释放整个父分配。
- Listener 采用已验收的拥有型事件策略。关闭后 Server 仍保留 Listener 引用，
  Drop 将原有绑定引用转交同 Worker 尾部任务；真实尾部退出后再退休 Listener／
  Server runtime／Engine 边。尾部任务内嵌、无分配，不增加线程或协程栈。
- 末次公开借用和退休选举使用同一锁；Prepare 仅投递真正的关闭命令，不在
  宿主 Prepare 栈内运行 Future 完成回调。完整联合计划仍需准备所有节点并保持代码租约。
- Accept Future 具有实际生产者、拥有型取消 Watch、Server 引用和 Stream 结果策略。
  构造者／队列、生产者和 Watch 分别持有真实引用，Next 仅为结构链接。
  取消当前等待不关闭服务；拒绝结果交付时 Abort／Destroy 无人接管的流。
- 关闭壳体可以比 Engine 活得更久。同步调用全过程保持活动借用，读取 Engine
  前先检查关闭状态；异步关闭结果也不通过已释放的 Engine 指针工作。
- 原始 Server／Stream 回调和 Data 的低层 C 能力保留，但完整拥有图继续拒绝
  不透明回调，不通过虚构叶对象准入。没有改动 Cgen 或重写网络算法。

## 最终证据

根目录：`F:/xlang-validation/five-groups-20261021/p3-server-owned`。

| 最终报告 | 阶段 |
|---|---:|
| native-win-r3/results.json | 231 |
| native-linux-r3/results.json | 288 |
| listener-regression-win-r1/results.json | 24 |
| listener-regression-linux-r1/results.json | 30 |
| 合计 | 573 |

GCC／TCC O0／O2、模块化／单头、select／IOCP 或 epoll，Linux 另有 ASan／UBSan。
全部输入、日志及产物哈希不变。回归重用同一批已逐项核验哈希的运行库对象，
仅重新编译 Listener／Stream 夹具，没有重新编译相同生产对象或改写原报告。

新夹具每次包含 6 次整图退休、13 次登记的策略／裸回调拒绝、15 个分配预算
（13 个失败前缀）、两个端点的 NUL 通信、9 次绑定／激活最终 Drop 竞态、
8 次取消／关闭竞态，以及真正释放 Engine 后的 Server 壳体调用。删除绑定父边
的离线代码变异被未申报外部根检查拒绝；core-only 链接不包含 Server Future。
旧接受、重入关闭、Future、同步、OOM 和拥有型 Listener／Stream 路径均在显式允许列表内。

验证只使用离线图／ABI／生命周期／故障注入和自建字面量回环正常功能，不执行
DNS、公网、畸形协议、攻击模拟或网络安全性质专项。早期发现和中间审查版本保留，
不计入上述最终通过；旧测试已改用实际 checked 退休，未添加固定 sleep 作为释放证明。

## 范围与下一步

本批仅在独立 XRT 仓库完成。生成头由干净已提交父版本加精确本批源码生成，
只提交选定 TCP／Server 区段，未纳入文件／内存工作区修改。
没有更新 xlang 固定 XRT／xruntime 头、时间 ABI、发布二进制或 VFS。

后续已完成 Listener＋Server 闭包的固定快照同步和驻留目录，保留微秒／绝对期限
合同，详见 [Server 固定 ABI 与目录](tcp-server-pinned-catalog.md)。下一步仍需
不可变多端点配置、聚合 Server 驻留资源、同步／异步接受、状态与统计及语言验收。
引用／接管发送、完整 UDP／多播、文件／网络任务组合、P0／P1 剩余项和 P4—P7
继续开放。未决策 CRT 0／XRT 11 不变。
