# xruntime 语言桥接契约

本文定义 xlang 使用新版 XRT 与 xruntime 的长期边界。目标不是把每一个 C 函数机械改写成命名空间函数，而是在不丢失底层能力的前提下，让语言内核、类型和标准库各自承担合适的职责。

## 三层能力

### 语言内核语义

以下能力由编译器生成代码和 `xl_runtime.h` 直接承载，不作为普通标准库对象重复包装：

- `type(expr)`、`is`、`as`、类型字面量和运行时类型身份；
- class、record、union、enum、protocol 及其类型描述符；
- 强引用、`weak<T>`、弱引用锁定和对象生命周期；
- class 固定字段、动态字段、方法表、协议见证和对象图追踪；
- `var` 装箱、拆箱、复制、移动、克隆、比较、散列和字符串转换；
- 函数值、调用帧、返回值、future 和 coroutine 的 ABI 桥接。

这些能力最终与 xruntime 的运行时类型、对象、字段、对象图、调用和值模块对应，但由语言语义保证类型安全、所有权和自动清理。不得再增加一套可绕过语言规则的高级包装。

## 当前内核接入状态

动态字段已经完成真实迁移：

- 根 `object` 的 `__fields` 保存 `DynamicFields` 语言对象，不再保存私有 `dict`；
- `DynamicFields.get/set` 使用 `xrtDynamicFieldsGetRef/SetRef`，保留同一 `xvalue` 身份，别名修改立即可见；
- `copy/clone/toValue/fromValue` 提供明确的深复制隔离路径；
- class 的 JSON/XLON/value 持久化只通过动态字段转换桥访问字段存储；
- 普通无 class 程序不注入 `DynamicFields/object`，需要该能力时自动闭合 `XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD`；
- 解释执行、生成 C 和 standalone 裁剪产物使用相同的 ABI 与模块边界。

class 实例与弱引用已经完成底层统一：

- 普通语言 class 由 `xrtobject` 承载身份和字段负载，语言类型描述按进程期惰性映射为稳定 `xrttype`；
- `weak<T>` 直接使用 `xrtweak` 的复制、移动、过期判断和强引用锁定，不再维护 xlang 私有控制块与互斥锁；
- `self` 反向装箱统一返回新的强 `xvalue` 引用，异步接收者、闭包捕获和 `var self` 使用同一所有权口径；
- record 与纯 native handle 继续使用轻量私有 box，不承担类对象身份和弱引用成本；
- 继承、动态派发、协议、异步存活、C ABI、析构顺序和 standalone 裁剪均由回归套件覆盖。

当前没有把语言 class 自动加入 `runtime_object_graph`。类实例的 `xrttype::InstanceOps.Trace` 暂为空，原始对象图 API 仍作为显式底层能力公开；在字段描述与动态字段都能无遗漏枚举强引用前，不把循环收集标记为语言内核已完成能力。

### 语言级抽象

适合日常开发的能力必须表现为类型或类型方法：

- `Buffer`、typed array/list/set/dict、`Stack<T>`、`AvlTree<K,V>` 和并发队列；
- `DynamicFields`，提供共享引用访问、显式深复制、合并、容量管理和 Value Object 往返；
- `future<T>`、`weak<T>`、线程、协程、任务、同步原语和网络对象；
- 字符串、时间、路径、文件、编码、JSON、XLON、模板等标准类型或命名空间。

语言级抽象应直接使用 XRT/xruntime 的所有权、批处理和零拷贝入口。不得在生成代码中重新实现容器遍历、集合运算、事务合并或生命周期算法。

### 显式底层能力

XRT 与 xruntime 的全部公共符号都进入 TCC 符号表。高级用户可以通过 `extern "xrt..."` 使用类型注册表、协议注册表、原始对象、字段偏移、对象图、调用帧等底层 C API。

这一层用于运行时扩展、FFI、编译器插件和自定义对象模型。它保留完整能力，但不承诺绕过语言所有权规则后的自动安全性。

## `type` 的边界

`type` 是稳定类型身份值，不是可变反射对象。当前稳定契约包括：

- 类型字面量与 `type(expr)`；
- 精确相等和不等比较；
- `string(typeValue)` 返回正式类型名；
- `var` 保存值的实际运行时类型；
- 泛型容器和函数值的精确类型身份。

类型描述符的大小、对齐、字段偏移和原始方法入口属于 ABI 与内核能力，通过显式 `extern` 保留。除非未来出现明确的运行时反射场景，否则不将这些布局细节提升为普通语言成员，以免冻结编译器内部表示。

## 外部原生 Array 生产者（B128）

`string.chars/split/splitLines/fields` 和 `RegexSet.hits` 直接构造原生 typed
Array。字符为 Unicode scalar `uint32`、字符串为拥有的双字段
`xstrview`、匹配索引为 `int64`；不再逐元素创建 `xvalue` wire 数组。
公开语言调用不变，私有桥接符号升级为 V2，不保留旧 V1 ABI 别名。

闭合注册表通过 `nativeArrayResultAbi` 声明结果构造合同。IR 核验精确
Array/元素 TypeUID、非 nullable 的 primitive layout、参数物理 ABI
及 NULL-result 失败观察；Cgen 只传入已经核验的两个 TypeUID，不根据
函数名字重新推断元素类型、转换 wire tag 或生成数组实现。

resident producer 在同一错误事务内创建 owner、安装 admission 和
projection、保留 owner pin 后写入真实 slot；失败时释放完整/部分
结果再传播独立失败位。String/char 的 descriptor/codec 由各自 producer
提供，通用构造器不依赖它们，因此整数结果不隐式选择 char 提供者。
动态 `var` 容器方法为 may-raise，必须在调用后观察 NULL-result 并进入
语言错误边，不能只在 runtime 设置错误、却跳过 `catch`。

验收门为 `tools/check_stdlib_array_producer.py`。它覆盖精确 NUL/UTF-8
输入、原生 slot/admission/projection、真实语言别名和错误捕获、伪造
IR 合同拒绝、旧 C 消费者、完整分配失败前缀及 JIT/AOT；Linux 可用
`--sanitize` 插桩所选原生路径。该门不代表全部 Array producer 或
53 模块总验收完成，文件系统/其他 wire producer 等仍单独收口。

## 并发队列的 resident 前置内核（B129）

`xl6_queue_spsc/mpsc/mpmc.c` 分别调用真实的 xruntime typed queue；共享
模板只在编译期实例化，不做运行时 kind 分派，不使用 channel、逐项盒子
或第二套环形队列算法。三个独立 artifact component 和 27 个私有出口
按需选择；这不是新增的公开标准库函数，也不表示旧语言入口已迁移。

构造器消费已封闭 Array/Stack schema witness，只保留独立拥有的完整
admission/layout 元数据，不保留 witness、元素、生成代码或模块回调。
每次访问核对实际 handle ops、owner/element TypeUID、大小及 resident
物理 descriptor；窄语言整数和原生 C 宽度不能互相冒充。

单项流控保留 OK/EMPTY/FULL/CLOSED；ERROR 独立于错误消息是否分配
成功。批量输入域检查在发布前完成，拷贝失败保留已经成功发布的前缀
数量，并明确报告 ERROR；不在前缀 OK 中吞掉无诊断失败。输出为原生
内部 ABI 的 fresh initialized slots，所有路径均由调用方负责清理；
后续 last-owner cleanup 失败不意味着已经移动的结果被回滚。

操作 pin 在短 mutation 中获取，实际拷贝/释放在其外执行。SPSC 只允许
一个活动生产者和一个活动消费者，MPSC 只允许一个活动消费者；角色
的顺序交接有 acquire/release 同步，不依赖可能复用的 OS thread id。
close/reset 与全部在途操作互斥，不为语言增加 scheduler 等待；原生
序列环的内部提交仍遵守 XRT 既有进度合同。reset 要求队列空且独占，
也允许重置尚未关闭的空队列。

对象图为 Value shell → counted owner → 全部固定初始化 slots，包含
重复引用、重试槽和 Future control block。活动 pin 拒绝快照/回收；
Future TraceSlot 还需精确 resident policy allowlist，未知边不能当叶子。
Clear 只在 freeze 中 detach；Finish 在 freeze 外释放。继承的原始异常
和本次物理完成结果分开观察，不能因为已经展开而无限重试清理，也
不能丢掉析构回调在 OOM 下的无诊断失败。

`tools/check_stdlib_concurrent_queue_native.py` 验证严格 C、实际并发
传输、精确 NUL/UTF-8、char/bytes/nullable/C-width、拥有图、错误与
完整分配失败前缀，以及实际 provider/原生 TU 的 family 裁剪。Linux
`--sanitize` 插桩所选原生路径；shared runtime/CLI/JIT 不因此成为全量
插桩产物。门已纳入 Stack 父门。

语言入口仍使用旧 channel 别名，其 wire batch 已不适用于原生 Array。
后续必须一起完成闭合 IR schema/效果/失败观察、直接 native 调用、
拥有返回值接口、退役输出写回接口及 JIT/AOT/continuation/图回收验收；
不得把本前置门的通过外推成 Queue 或三组标准库封板。

## typed 队列的原生 Array 批量合同（B130）

三种独立队列 provider 增加私有 `PushArrayV2` / `PopArrayV2`，仅是语言
切换所需的 resident ABI，**不是已经完成的 xlang 队列接口**。旧 Cgen
channel 别名和旧 `Into` 形参仍待整体退役，不能用本节原生验证代替语言验收。

- 输入必须是真正的原生 Array：角色、逻辑元素身份、完整 admission、
  nullable/nominal/子 schema、物理 descriptor 与队列一致。取得 Array
  操作 pin 后直接读取连续 typed storage；先验证整个输入，再允许发布
  第一项。拒绝同宽但不同逻辑域、伪造身份、BUSY 或 wire Array。
- 批量循环只存在于 resident 内核，共享同一精确前缀实现；不生成
  逐项装箱、wire-array 读取、转换或 channel 适配代码。
- 出队结果是 `{Result, Count, Items}`，Items 是独立拥有的原生 Array，
  不接收输出参数，不修改已有容器或其别名。它复制队列的完整、独立
  拥有的 resident admission 元数据；输出 Array 身份与 projection 来自
  已验证的闭合 IR recipe，而非运行时名称推断。
- 必须先构造最终 wrapper、绑定 resident projection 并预备全部初始化
  传输槽，再消费队列。预备失败不消费任何项；过程中发生 ERROR，Count
  和 Items 仍拥有成功移交的前缀，异常路径必须释放 Items。错误结果独立
  于是否能分配诊断，不得把真实错误吞成普通部分成功。
- `maximum` 非负、是上界，内部至多预备队列固定 capacity 个槽，不依赖
  并发近似 Count，也不承诺继续等待后来生产的项。零上界返回拥有型空
  Array / OK；普通空队列和关闭排空仍精确返回 EMPTY / CLOSED。
- 未发布、BUSY 的内部传输目标不是语言默认值；只有成功移交的前缀
  可见。非 nullable Future 不凭空变成 null；bytes 不为暂存目标构造
  无用的空 bytes。结果依然保持各自的公开 resize/default 合同。
- 引用传输只接受空 projection 参数，内部选用统一的 resident retain
  codec，保持原对象身份，绝不落到 `xrtTypeValue` 动态 COW 克隆。
  其他 projection 是无 Context 的 immutable resident 元数据，不允许
  存储 generated-code callback。graph 的 pin、退役与 Future policy
  allowlist 沿用实际物理节点，覆盖 Queue/Array/Future 互相拥有的循环。

验收包含 Windows O0/O2、Linux O0/O2/ASan/UBSan、完整分配前缀、
单项与原生 Array 批量的真实并发，以及 TinyCC 调用 resident GCC 的
16-byte batch / 24-byte owned Array result 结构体 ABI。TinyCC 状态必须
由创建它的同一 provider 配置，完整 source registrar 先于旧 DLL 默认
绑定；实际删除 TinyCC 后结果 Array 仍可读取和释放，且产品分配平衡。
这些证据不证明语义/IR/Cgen 切换、正式部署或三组 53 模块已完成。

## 非阻塞队列的闭合 XLIR schema（B131）

`SpscQueue<T>`、`MpscQueue<T>`、`MpmcQueue<T>` 的 family 由
`runtimeResourceAbi` 明确表示，`concurrentQueueSchemaType` 指向规范化的
`Array<T>` 类型。该 schema 在 Typed XLIR 构建和泛型/类成员类型闭合时
建立，不依赖源程序是否创建过数组；规范化调用复用同一 TypeId。
队列自身不是 Array/Stack，`sequenceLayout` 不被伪造为容器 role。
WaitQueue、可空队列 owner 和非队列类型没有这一字段的有效值。

`xl6IrConcurrentQueueConstructionRecipe` 只在 owner、family、规范 schema、
元素身份、完整 Sequence 布局与预计算 nominal admission set 都闭合时
成功；未闭合的可保留指针仍然 pending。失败不写任何输出参数。
Lowering 明确绑定构造的 admission set，IR verifier 和 Cgen 构造边界
重新核对，篡改须在输出 C 前被拒绝。物理槽位相同也不是逻辑域可互换
的依据；callable 的 closure/value codec 与 Future 的原生槽位仍保留。

本批 Windows/Linux O0/O2 验证了 60 个完整 schema 的真实 Sequence
工厂→对应原生空 Queue→owned 空 Array→清理生命周期，960 次 schema/布局
篡改、9 次嵌套篡改，以及真实构造指令的拒绝边界。它们是原生迁移的
IR 前置证据，不是公开 Queue API 已完成迁移的证据。旧 Into、channel
别名、wire batch、结果/error 合同和 Queue graph resolver 仍需切换。

## 公开原生 Queue 与 Tree→Array 边界（B132）

本节更新 B129–B131 的历史状态：三种非阻塞队列已经切换到独立 typed
provider，不再以 channel 别名实现；WaitQueue 的阻塞接口不在此次切换范围。

| 非阻塞队列方法 | 语言结果 | 边界合同 |
| --- | --- | --- |
| `tryPush(value)` | `int` | `queueStatus.ok/full/closed`；真正错误抛出 |
| `tryPop()` | `(int, T?)` | `ok/empty/closed` 与拥有的值分开；成功 null 不等于空队列 |
| `pushBatch(array<T>)` | `(int, int)` | 状态与成功前缀数；已有不同元素域的 Array 不重新标记 |
| `popBatch(maximum)` | `(int, array<T>)` | 独立拥有的原生 Array；非负上界，失败结果不泄露拥有前缀 |
| `close()` / `reset()` | 无返回值 / `bool` | 隐藏 checked 状态独立观察；reset 失败抛出 |
| `count/capacity/isClosed/isDrained` | `int/int/bool/bool` | 精确 family/身份检查；非法 owner 不能吞成普通查询结果 |

公开状态为 `ok=0, empty=1, full=2, closed=3`。已有成功前缀的普通部分批量
返回 ok；仅零前缀停止返回 full/empty/closed。零请求返回 ok 和拥有型空结果。
原生 ERROR=-1 不是公开业务状态：即使错误对象分配失败，也必须走语言失败边。
三种队列的 `tryPopInto/popBatchInto` 已在语义阶段拒绝；不得恢复写回参数。

同一闭合 IR 合同贯穿签名、lowering、verifier、Cgen：逻辑元素身份、物理
槽、transfer/codec、完整子 schema、nominal admission set 和效果都须一致。
每个原生调用之后有独立 ERROR_TEST。Cgen 只渲染直接调用及拥有者/错误边界，
不产生 wire Array 批量循环、动态 family 分发或 channel fallback。
闭包的语言调用指针与 resident Value-box 槽位不是同一 ABI；Future 仍使用
原生 Future 槽位。slot descriptor/converter 与裁剪根来自同一静态登记。

`AvlTree<K,V>.keys()/values()` 的结果也已经是完整 recipe 构造的原生 Array，
由 `xl6TypedTreeProjectTakeV1` 对精确 typed storage 做有序快照。空树、内嵌
NUL、UTF-8、nullable char、nominal 引用及嵌套 Array 均不经过 wire 转换。
字符串复制成独立值，引用元素保留同一对象；Tree 存储、get/clone 和投影
使用与 IR transfer 一致的 retaining descriptor，不能用逻辑 Value 类型的
COW clone 冒充语言引用复制。原生投影消费 fresh seed 的所有路径，失败
退役未发布结果、保留首错且不修改源树。生成代码不再含 `xlir_avl_collect`。

此边界修复不等于 AvlTree 全部方法迁往 resident owner：旧生成 handle 的
图追踪、部分 no-raise 方法效果及整个生命周期仍须另批统一收敛，不为共享
reference descriptor 添加伪造的全序比较器。宿主 Queue/Sequence resolver
已经接线，但语言级循环图、continuation、调度器级 OOM 与 53 模块总验收
尚未完成。当前证据及复现命令见《三组标准库接入长期任务》B132。

## 发布门槛

- 仓库快照中的 XRT+xruntime 公共函数必须全部进入 TCC 符号表；
- 容器和任务高频路径必须直接调用对应的 typed/batch API；
- xruntime 内核模块不得出现第二份语言级生命周期或容器算法；
- 被迁往 `xhttp`、`xws`、`xregex` 的旧符号分组保持为空，任何重新引用都必须使门禁失败；
- 能力矩阵、桥接守卫、解释执行和静态产物测试必须同时通过。
