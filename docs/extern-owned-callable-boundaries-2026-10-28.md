# C 字符串链路：方法绑定与异步退役复核修复

本轮修复 `review-boundaries-86/report.json` 中 F1–F6。实现基于已提交的主线
`4bb7d3ee8`，在独立工作树完成，随后合入主线 `ea38eff39` 及仅含 IDE 文档的
`d7d7eef7d`；其他线程的未提交改动由合并校验单独保留。
不改变 string/cstring/cbytes 的运行时表示，不改变 XRT 的分配器或自 join 合同。

## 阶段提交

| 提交 | 修复 |
| --- | --- |
| `5e55a4815` | base 静态分派、泛型异步方法绑定、重载排序、nullable 函数目标 |
| `71665e991` | 先推断不变的结构见证，再验证直接 nullable 值实参 |
| `b0a26ede3` | 最后一个 producer 引用释放时，保留原生任务池的收尾活动 |
| `5fc2b9987` | 泛型方法约束检查、约束不成立的重载排除、future<null> 绑定 |

## 行为与边界

- `base.Read` 和 `base.Read()` 都静态调用基类；普通虚方法引用继续虚分派。
  closure 的分派槽由源接收者确定，verifier 同时核对计划和调用指令。
- `async func Echo<T>(T value) as T` 可绑定为 `func(string) as future<string>`。
  推断比较逻辑返回层，实例化后按实际 Future ABI 验证。无返回值使用现有
  `future<null>`，不引入新的 void 类型。
- 固定签名函数值采用与直接调用相同的转换成本、默认实参数量排序；真正的
  同级歧义仍拒绝。约束组先在语义阶段检查 `T is Base` 等条件，记录唯一声明；
  Lowered XLIR 只允许该声明及其具体化链，不能重新选择约束不成立的候选。
- `Reader? cb=box.Read` 支持初始化、重新赋值、参数和返回边界。先建立原有
  typed closure，再进行保持同一指针的 nullable 提升；不创建第二个 closure。
- `choose(value,array<T> witness)` 和反向参数顺序采用相同推断。结构类型先固定 T，
  直接值可使用原有 nullable 赋值转换，包括 Child 到 Base?。`array<Child>` 不会
  转成 `array<Base?>`；命名参数及实际求值顺序保持不变。
- 退役首次错误经只读诊断版本定位到原生 worker 上的 `xrtTaskPoolTryJoinV1`。
  producer 回调可能晚于 Run/Drop/Watch 活动结束，末次代码 lease 释放会重入宿主
  退役；旧实现丢失了这段任务池活动。现在在该末次释放的宿主回调返回之前保留
  一个真实池引用及活动记录，预备阶段返回 BUSY。没有把 XRT 的 ERROR 改成 BUSY，
  没有清除真实清理失败，也没有在正常驱动中增加失败重试。

## 成本

普通同步 native 方法绑定继续使用既有 bool/typed-out 入口和一个接收者捕获。
重载、约束、异步结果归一化及 nullable 证据均在编译期完成；没有增加字符串
来源标签、读取分支、结果装箱或转发 thunk。回归仍要求 `@owned string` 的
生产者原指针以及首次调用只包含一次生产者分配。

新增运行时工作仅发生在“最后一个 frame 引用经 producer 回调释放且带 pool/lease”
的收尾路径：一对现有 ActivityEnter/ActivityLeave，包含短锁、计数及一个池引用的
保留/归还，无堆分配。其他 frame 释放不增加这对活动操作；同步字符串路径不受影响。
这项安全修复不应被描述为整个异步调用与裸 C 等成本。

`Future<string>` 仍是共享结果，读取继续使用既有复制规则。异步方法回归分别检查
producer 内部的原指针接管和 Future 读取结果；不把“C 返回值已接管”描述为
“Future 读取也零复制”。改变共享字符串结果的生命周期/写入规则需要单独设计。

`@owned string` 的分配器前提仍是实际的 xrtMalloc/xrtFree 兼容性。相同 CRT 或编译器
不意味着任意 malloc 指针可交给 xrtFree；任意释放器继续使用 cstring/cbytes 路径。

## 回归与复现

耐久门为 `tools/check_extern_owned_string.py` 及
`tools/check_extern_owned_string_public.py`，新增覆盖六组正向程序和四组反例。
`xlir_bound_dispatch_contract.c` 拒绝 6 项分派元数据/调用指令篡改。
`xlir_task_producer_tail.c` 在真实原生 worker 上确定性释放最后的 producer 引用：
旧 frame 实现失败，修复后 64 次通过，其中 32 次回调禁止分配；每次宿主释放均
在 RC mutation 外运行，最终分配/释放数量及存活字节严格平衡。

原随机退役程序在未插桩的 Windows 验证编译器上连续 160 次通过。诊断版本和
旧版失败日志独立保留，没有覆盖正常编译器及其构建收据。

每个平台的最终矩阵由以下阶段组成：公开编译接口 194、独立 C/IR 33、任务组合 18、
上一轮边界回归 108、构造合同 8、本轮原始审计 104，共 465。Windows 另验证了
原生指针集合合同，覆盖 41 阶段、46 项拒绝的 IR 篡改与 22 个平衡的分配失败。
平台实际状态及输入/产物哈希以验收文件为准，不从构建成功推断测试通过。

Windows 完整矩阵使用 `driver-90`，Linux 使用 `linux-90`。Windows `driver-91`
只重链相同的 1296 个已验证对象，补齐历史收据 Sources 列表遗漏的已编译文件；
原收据和测试结果保留不动，指针集合合同使用补齐的收据重新验证。
发布目录中的编译器二进制没有被替换。

Linux 补充矩阵初轮有一项 AOT 构建超过 30 秒。该构建及后续阶段在新环境使用
240 秒限时复验，原超时日志与成功结果分别保留；不复用超时产生的二进制。

合并主线后的唯一生产代码差异，是 `ARRAY_EXTRACT_VALUE` 的裁剪产物补入
`ERROR_VALUE` 提供器。使用 `driver-92` / `linux-92` 重新构建后，各完成 22 项增量
检查：Error 提取/传输、约束绑定、base 分派、nullable callable 的 C/JIT/AOT，
以及包含 1,567,504 对依赖的产物注册表与 IR/裁剪合同。合计每平台 487 个验证阶段。
其余主线增量为测试与文档；未把其他线程尚未提交的工作计入本轮验收。

验收归档：`demo6/build/owned-string/callable-boundaries-90/acceptance.json`。
