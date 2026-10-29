# Stack 动态批量操作与相等性

2026-10-14。经 `var` 调用 Stack 的 `pushBatch`、`popBatch`、`peekBatch`、
`peekBatchAt` 和 `equals` 已接入统一 IR 与驻留提供方。实现复用现有
typed Stack 和 Array 存储，不在 Cgen 拼接标准库函数体。

## 类型与拥有合同

Stack 擦除指令显式携带 `sequenceResultArrayType`，表示同一元素类型的
规范 `array<T>`。IR 核对完整构造配方、精确角色、元素类型、槽 ABI、
传递策略和编解码合同；缺失或不匹配的结果类型在 C/header 输出前拒绝。
文本和 JSON IR 都展示该字段，其他操作不得携带它。

生成代码调用 `xl6StackEraseTakeV2`，消费源拥有值，并向原生对象绑定
确定的结果 Array 身份。绑定只允许在 Stack 操作 pin 下进行，不能
换成另一身份；克隆保留该身份和原有接纳／投影策略。原生旧入口没有
结果身份时，批量操作报 StateError，不通过物理布局或名称猜测类型。

`pushBatch` 要求精确的 Array 身份及元素合同，复制输入元素，不消费
输入数组。读取批量结果是拥有的独立 Array，按栈顶至栈底排列；最大
数量超过可用元素时截断，负值及超过栈长的 depth 报 RangeError。
depth 等于栈长返回空数组。准备阶段失败不改变源内容；Take 完成
移交后不再分配或绑定结果元数据，晚来的清理失败仍须报告，不能声称
已经回滚原生提交。

`equals` 要求同一 Stack 身份和完整接纳策略，复用 typed 相等性；
同类型但内容不同正常返回 false，其他类型报 TypeError。动态调用
参数只由外层调用事务消费一次，提供方借用输入并返回一个拥有值。

批量返回值可以通过现有动态索引和 `.length` 使用。`var as array<T>`
尚未接通，不包含在本批完成范围；不能以动态内容检查代替该转换的验收。

## 双平台验收

完整源码构建为 Windows 330、Linux 329 个翻译单元。两端各 85 项
检查覆盖七份夹具的确定性 C、严格 C11 O0/O2、JIT、module、独立
AOT，以及 NUL、UTF-8、char、bytes 值语义、nullable 和对象引用。
纯 typed Stack 的 AOT 不包含新的擦除／动态批量提供方。

九处擦除边界的 81 个 IR 变异均在 C/header 输出前拒绝。每端 O0/O2
分别扫描核心夹具 85 次和批量夹具 230 次分配预算，直到首次未触发，
并完成 init/main/fini 与拥有账本核对。这是同步调用线程前缀，不代表
工作线程或所有对象图的 OOM 验收。

既有 typed Stack 批量接口补充回归每端 37 项，覆盖公开接口、值域与
资源三份夹具的严格 C、JIT、module 和 AOT，包括原生数值、enum、
thread、OrderedMap、Queue 与嵌套 Stack。证据为对应的
`typed-win-final1` 与 `typed-linux-final1`。

原生专项每端 71 项检查、12 条闭合分配前缀及五个实际错误实现对照
通过，Linux 对所选实现启用 ASan/UBSan。额外验证克隆的 Array 身份、
不可重绑定、角色拒绝，以及已产生返回对象但同时出现独立失败时的
完整释放。错误对象为 NULL 的首个失败不会被晚来的诊断替换。

语言证据为 `E:/xlang-validation/stdlib53-b274-language-win-final1/results.json`
和 `/var/tmp/xlang-stdlib53-b274-language-linux-final1/results.json`；原生
证据为对应的 `native-win-final1` 与 `native-linux-final1`。复用构建前
逐项核对源码、头文件和对象摘要；旧 TCC 仅提供真实 C 编译前置。

## 后续范围

Stack 动态 try 的多返回值、动态容器类型转换、其他内建容器与 Native
编码、工作线程／对象图故障、模块组合及发布总门继续。此次源码专项
不更新已发布 CLI/DLL/VFS，也不宣告 53 模块完整封板。WaitQueue
保持已批准的 MPMC 和 `(status, T?)`；CRT 未决策为 0，XRT 19 组不变，
对象池仍最后处理。
