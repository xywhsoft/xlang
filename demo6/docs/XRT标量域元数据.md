# XRT 标量域元数据底座

本文件描述通用的不可变元数据底座、嵌入快照及 xlang 的语言侧使用合同。
原生来源为 XRT 提交 `43d6309b84e808e5f01e8d2588a524cc68f62b57`。

`xrtValueScalarDomainBindV1(value, typeId, domain)` 原子提交语义上的身份二元组；
不是供并发发布使用的硬件原子写。首次绑定必须唯一、尚未发布，调用者同步发布。
`xrtValueScalarDomainV1(value)` 读取调用者定义的 32 位 POD 标签。
两者无登记表、回调、额外分配或代码生命周期依赖。

共享后只允许幂等地绑定原有二元组；不同标签、不同身份或共享后首次绑定均拒绝。
容器、Handle、静态单例以及 BUSY／退役对象不能绑定。失败不改变原值。
已绑定标签的标量不允许通过 TypeIdRebind 换成另一身份。
XRT 不解释位宽、有符号性、枚举等语言含义，不改变 XRT 的相等、哈希和序列化。
标量本身不可变，Clone／DeepClone 保留同一外壳，包括容器中的深复制路径。

在已验收的 Windows/Linux 64 位 ABI 下，标签占用 TypeId 前的 4 字节对齐空隙；
外壳仍为 88 字节，TypeId/Data 偏移仍为 16/64。其他 ABI 不保证存在此空隙，
应用仍不得依赖不透明 xvalue 的内部布局。

原生矩阵包括 value 与 value_graph 两种闭包、模块化与单头、GCC/TCC O0/O2，
以及 Linux ASan/UBSan。旧值模型、句柄、图复制、OOM 回归与新域合同一起验证。
原生仓库中无关的内存和 file_root 工作区变更未纳入此提交或本次同步。

xlang 仅同步 value 公共声明、内部布局及 value.c 三段，保留其他快照差异。
核心快照现有 3244 个公共函数，xruntime 为 3709 个（自有 465），新增函数 2 个。
`tools/check_xrt_scalar_domain_snapshot.py` 在实际嵌入的 core/runtime 两种产品上，
验证实现 TU 与独立 SDK 声明 TU、GCC/TCC O0/O2、元数据保留、清理与布局。
不承诺先包含声明头后在同一 TU 重新选择实现特性；提供者和 SDK 使用方分 TU。

## xlang 使用合同

语言层划分枚举、C 标量、基础值三种版本化命名空间，低字节记录精确的标量域。
XRT 不依赖这些编码。IR 使用独立 checked `XVALUE_ENUM_BIND`，不允许枚举继续走
旧的裸 TypeId 绑定；驻留 runtime 根据完整身份／位宽／符号执行动态位运算。
未知身份或缺少域标签的绑定值按 checked 错误拒绝，不从名称、数值或 AST 猜测。
普通动态运算也迁至驻留 provider；Cgen 仅声明、调用和清理，不再输出算法函数体。
加法的字符串／Array 路径另设 provider，纯数值路径不拉入 Array 或网络依赖。

`XL6_SequenceAdmissionV3` 在既有 64 位对齐空隙中增加 `scalarDomain`，大小仍为
56 字节，原字段偏移不变。所有源码初始化列表同步更新；策略复制、相等判定和
Array／Stack／List／Dict／Set 等擦除投影传递同一标签，不添加登记表或源代码引用。
bool／null 的 XRT 永久单例不被改写。快照先绑定全新标量，再将拥有权移交数组；
绑定或追加失败则释放整个未发布快照，不交付半成品。

源码驱动的 JIT／模块／独立 AOT、严格 C、畸形 IR 和 provider 裁剪检查分别由
`tools/check_numeric_enum_bitwise.py` 与 `tools/check_numeric_value_runtime.py` 记录。
这些验收不等于发布 DLL／SDK 或长期任务的最终联合验收。
