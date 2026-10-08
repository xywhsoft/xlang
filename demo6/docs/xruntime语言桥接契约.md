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

## 发布门槛

- 仓库快照中的 XRT+xruntime 公共函数必须全部进入 TCC 符号表；
- 容器和任务高频路径必须直接调用对应的 typed/batch API；
- xruntime 内核模块不得出现第二份语言级生命周期或容器算法；
- 被迁往 `xhttp`、`xws`、`xregex` 的旧符号分组保持为空，任何重新引用都必须使门禁失败；
- 能力矩阵、桥接守卫、解释执行和静态产物测试必须同时通过。
