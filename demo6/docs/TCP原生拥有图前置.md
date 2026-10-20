# TCP 原生拥有图前置

本批是 P3 真正传输接入的生命周期前置，不是 TCP／Dial／Server／UDP 标准库完成声明。
原生实现来自 XRT `5a12e8f3` 与 `184edd63`，只同步明确选定的网络段。
未同步 XRT 工作区的文件／内存库改动，未替换发布 DLL／VFS。

## 已落实

- Engine 的侵入式运行时槽真正接管构造时已有的 TCP 引用；摘除时消费同一槽，
  不增加第二份引用，不以计数器推测引用关系。
- Stream／Listener 对 Engine 的真实占用、Listener 接受队列每个位置的 Stream 引用、
  连接超时和接受重试计时器已有的独立引用全部入图。队列／哈希链接不是额外拥有边。
- Stream 的 Error／Cause 诊断链按真实引用导出；不可变错误 DAG 不会反向引用 Stream，
  因而和原生 Error 的不可变尾部一样保留到最后 Drop。静态错误不伪造图节点。
- 原生任务实际排队、活动 worker、构造发布、发送提交、Future 创建／取消等不稳定区间
  明确报告忙碌；不在 worker 仍执行时遍历可被其释放的队列或非原子字段。
- Hold／Drop、Claim／Restore 和 Clear 使用真实引用及唯一令牌。新准入受隔离，
  已有强拥有者不会被伪造引用或暗中解锁替代。
- 非等待 Prepare 使用嵌入式命令借用原运行时槽，不新建根引用；排队成功不是 Ready。
  在命令开始前保留实际运行时引用及必要 Engine 占用，发布失败在冻结内回滚。
  端口完成、计时器终态、缓冲归还、通知和槽摘除完成后才报告 Ready。
- 准备可能使普通关闭产生新的原生错误。验证程序重新冻结并发现、准入、保活新 Error
  节点，不沿用旧快照，也不忽略新增拥有边。

## 低层接口与准入边界

新增低层入口共九个：

| 原生入口 | 作用 |
|---|---|
| `xrtNetEngineOwnershipAdapterV3` | 在原任务／计时器／线程退出策略之外，精确认证原生运行时族 |
| `xrtNetStreamOwnership`／`xrtNetListenerOwnership` | 借用物理拥有视图 |
| 两种 `RuntimeOwnershipV1Get` | 常驻、不可变的运行时槽与原有计时器合同 |
| 两种 `OwnershipQuiescenceV1Get` | 冻结下只读报告瞬时忙碌 |
| 两种 `OwnershipAdapterV1` | 实际保活、隔离与非等待准备合同 |

旧 Engine V1／V2 不会自动认可新的运行时族。当前语言 Engine 拥有域仍使用原有严格
准入，不提前把完整 TCP 语言资源当成已经实现。九个入口进入快照／TCC 低层符号面，
不是新增九个面向用户的标准库方法；语言 Cgen 没有增加临时包装函数体。

当前原生 V1 只接受能够完整证明的无任意回调／无 Data、空缓冲、无活动 Future 等待
状态。尚未建模的缓冲、Future、拨号控制器和回调载荷继续明确拒绝，不能套用叶子策略。
借用视图不是保活引用；任何协调器仍须遵守冻结、精确准入、真实 pin、重新证明不可达、
Clear 和最后 Drop 的完整顺序。普通 C 使用者仍须确保 Engine 销毁时外部使用者静止。

## 保留 xlang 固定 ABI

xlang 现有原生快照仍使用 `uint64` 微秒／`xdeadline`；上游 XRT 新的毫秒／实数期限
接口没有整库导入。原有时间字段、零／无限期限规则和转换代码保持原样。
公共 Net 新声明的上下文与上游期限签名相邻，故以精确唯一上下文单独插入，未放宽同步
工具的拒绝规则。TCP 两个相同回滚分支按冻结上游的精确出现次数同步。
其他修改使用现有具备旧字节检查的分段同步工具；Error 边也是独立的限定源段更新。

## 验证

证据根目录：`F:/xlang-validation/five-groups-20261021/p3-tcp-ownership`。

- 上游最终 `windows-r5`／`linux-r5`：184／230 阶段。模块化／单头、GCC／TCC
  O0／O2，Linux 另含实际选定原生闭包 ASan／UBSan。保留原 Engine 拥有图、绑定、
  连接计时器、关闭回调与回环接受回归。
- 每次新图执行包含 12 个生命周期图、3 个原有计时器引用场景、6 个 Future 取消后
  重新准入、3 个显式 Error／Cause DAG、12 个策略身份拒绝及 8 个构造 OOM 前缀。
  两端 select 与各自 IOCP／epoll 均执行，全部分配账本平衡。
- `pinned-win-r2`／`pinned-linux-r2`：12／15 阶段。同一真实图夹具只转换 include 和
  微秒／`xdeadline` 调用，直接编译 xlang 当前单头的实际选定闭包，包含 Linux sanitizers。
- `source-win-r2`／`source-linux-r2`：实际完整源码驱动，722／721 个翻译单元；
  `language-win-r1`／`language-linux-r1` 继续检验既有配置、代理值、分帧值和 signal
  的语言／JIT／模块／严格 C／独立 AOT、合同拒绝、裁剪与拥有分配失败路径。

上述源码驱动和专项通过不等于重新签发 shipping DLL／VFS 验收。只使用离线图与
自建的字面回环、动态端口、普通连接／关闭，无公网、外部 DNS、扫描、畸形流量或
网络安全性质专项测试。早期 r1／r2／r4 发现记录保留，不能计成最终通过证据。

## 后续依赖

下一步仍是带缓冲与 Future 的完整原生传输拥有图、受管理 Stream／Listener／Dial／
Server／UDP、取消／期限／半关闭／背压、代理及文件／任务组合；随后继续非对称密钥、
X.509、TLS 和双平台联合验收。P3 与总长期任务保持 active。
标准库未决策分类没有变化；参见标准库决策清单，CRT 0 项、XRT 11 组。
