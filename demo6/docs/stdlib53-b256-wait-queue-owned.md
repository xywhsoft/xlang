# WaitQueue 拥有接收与原生接入

WaitQueue 已贯通语义检查、统一 IR、独立 MPMC 原生底座和 C 输出。
公开接收返回 `(status, T?)`，不再写回调用者变量。旧 Into 接口明确
拒绝，旧生成的 Channel 队列算法已删除。本批专项和三组批量回归
通过相应门禁；嵌套编译器 SDK 失败、专用宿主及发布总门仍保留。

## 公开接口

| 方法 | 结果 |
| --- | --- |
| `tryPush(value)` | `queueStatus.ok/full/closed`，真正失败抛出 checked 错误 |
| `tryPop()` | 立即检查元素，返回 `(status, T?)` |
| `pop()` | 等待元素或关闭，返回 `(status, T?)` |
| `popTimeout(timeoutMs)` | 在一个单调截止时间内等待，返回 `(status, T?)` |
| `close()` | 唤醒等待者，保留已排队元素供排空 |
| `count/capacity/isClosed/isDrained` | 独立原生查询 |

`queueStatus.timeout` 增加为 4，其他正常状态值保持 0/1/2/3。
可空元素的成功 null 与空队列依靠 status 区分，不重复增加 nullable。
负超时是 checked ValueError，零超时返回正常 timeout。try 表示不等待
元素或容量，但仍可能等待内部 mutex，不宣称 WaitQueue 无锁。
容量必须为正，遵守 typed MPMC 的容量规范，没有零容量会合语义。
队列没有 reset 或批量公开操作。

`tryPopInto/popInto/popTimeoutInto` 不保留兼容别名。接收者可以是
已验证的 WaitQueue 表达式，包括容器中取得的拥有对象，不强制先
另建具名变量。其他数据结构的既有接收者规则未在本批扩大。

## 声明和统一 IR

公开名称、等待模式和超时参数来自同一接收声明表。WaitQueue 和
其他 concurrent queue 共用明确的消息 schema、准入集和拥有结果
合同，但保持独立原生 family。泛型实例必须关闭完整 schema 后才能
执行；类消息的名义准入集、callable 存储 codec 和 Future 传输 ABI
均由 IR 决定，不由 Cgen 回查源表达式。

接收者和超时操作数是观察，不消费调用者对象。结果可以为普通值
复制或独立拥有移动，checked 错误与正常 status 分开。对管理值，
已取得但失败的结果在保留原始错误的清理作用域中释放。可空标签在
成功之前保持可清理状态，避免已移动的管理记录被错误地当作 absent。

C 输出只装配声明头、精确槽位、原生调用和清理。原生 status 使用
enum 局部槽位，再转为语言 int，不把 int64 的地址当成 enum 地址。
callable 投影适配器按已关闭的基础签名选择，Future 保持原生引用。
嵌套 `array<WaitQueue<T>>` 只复制不可变准入元数据，不保存语言回调。

## 旧生成实现清退

`xlir_channel_handle` 和 `xlir_special_queue_*` 的生成算法、旧 Channel
needs 标志和生成队列 fallback 已删除。原生 Channel 的现有公开接入
仍保留，不删 XRT 的 Channel 功能。两处旧支撑文件净移除 412 行、
29169 字节（统一 LF 后）；这不是整个 Cgen 或发布 DLL 的体积测量。

旧 WaitQueue 夹具已迁移到状态和拥有结果，保持原有并发和 async
断言。Stack 的直接 `pop()` 返回合同未改变，不与 WaitQueue 同名
方法混用。

## 验证

Windows 和 Linux 完整源码编译器分别由 326 和 325 个翻译单元构建。
两端拥有接入专项各通过 52 阶段：五份程序进行 O0/O2 严格 C、JIT、
module 和真实 AOT 执行，覆盖泛型、string/NUL、bytes 值副本、char、
类引用、callable、可空 Future、嵌套容器、并发与 async。

两份真实 IR 共检查 33 个 WaitQueue 调用、13 个接收和 145 项合同
篡改，损坏模块被验证器、C/header 输出和 artifact plan 拒绝。
三项语义错误被拒绝。实际按需计划选择独立 WaitQueue 提供方，
未调用函数的计划、JIT/module/AOT 均证明不引入 WaitQueue 实现。

原生底座复测为 Windows 55／Linux 72 阶段，保留四生产者四消费者、
Future 循环图、完整创建/发送分配前缀、Sync 失败恢复、三个坏实现
拒绝及选定 Linux ASan/UBSan 验证。目录为 287 个组件、1469 个精确
导出，13 个坏目录控制通过。

三组批量清单扩为 301 个夹具；双端声明/严格 C 全部通过。双端实际
运行仍只失败 `stdlib_string_bridge_boundaries` 的嵌套编译器 SDK 部分，
没有新增失败。八个专用宿主或平台/产品条目不计为执行通过。

证据位于 `E:/xlang-validation/stdlib53-b256-` 前缀下的
`owned-{win,linux}-dev9`、`native-{win,linux}-dev9`、
`discovery-{win,linux}-dev9`、`runtime-{win,linux}-dev9` 和
`resident-{win,linux}-dev8`。诊断性单文件替换构建不计入本验收；
正式门禁只接受完整源码构建和逐项校验的消费快照。

## 后续验收

独立 borrowed 原生 SSA、工作线程和图 OOM、更广的模块卸载/错误
压力、专用宿主汇总、嵌套 SDK 以及发布 DLL/VFS 总门继续。本批不
代表完整 53 模块或所有函数发布完成。CRT 未决策为零，XRT 其余
19 组保持未决策，对象池最后。
