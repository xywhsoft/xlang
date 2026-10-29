# 动态函数多返回值的拥有合同

2026-10-14。通过既有 `xcall` 装箱的动态函数已接通多值声明、赋值和
丢弃字段。接收上下文给出结果数量，统一 IR 记录精确的 product；
各字段仍为拥有的 `var`，不按函数或方法名称猜测字段类型，也不把
多返回值包装成 Array。

## 语义与统一 IR

`auto left, right = invoke()` 的接收数量建立规范 `tuple<var, var>`，
后续 `left, right = invoke()` 使用同一规则。静态函数多返回值和
普通动态单值表达式保持原合同；typed func 仍通过显式 `xcall` 进入
动态 callable，没有放宽隐式函数装箱。

新增 OWNED_PRODUCT 结果 ABI 验证接收上下文、精确数量、所有字段
的动态值类型和管理方式。调用继续借用 callable，消费已装箱的参数；
结果 ABI 与参数装箱类型明确分离。失败物理 ABI 为 STATUS_OUTPUT，
不通过聚合体零值或错误对象指针判断成功。

C 输出先清零私有 product SSA 输出槽，再建立字段地址表，调用驻留
`xl6DynamicFunctionProductTakeV1`，以独立成功状态进入错误边。成功
之后由已有 PRODUCT_TAKE 和拥有赋值接收每个字段。库调用和清理逻辑
属于按需原生提供方，不作为新的标准库函数体写入 Cgen。

## 原生结果事务

提供方要求两个及以上、互不重叠且初始为空的私有输出槽。原生返回
数量必须完全一致：不足及超出均报 TypeError，不能默默丢弃额外值。
所有字段的拥有引用、参数释放和原生结果清理完成后才发布输出。
失败保持全部输出槽为空；数量错误发生在目标变量更新之前。

结果暂存复用四个 inline 指针，较大 product 使用独立临时区。输出
暂存分配在调用函数之前完成；函数本身的副作用不因随后数量不匹配
而回滚。输入参数和临时结果无论成功或失败均完整释放。逐个结果
析构捕获错误，保留首个失败，包括无错误对象的失败；晚来的诊断
不得替换它。

## 验收结果

Windows 332、Linux 331 个翻译单元的完整源码构建通过，无对象替换。
语言专项两端各 85 项，覆盖确定性 C、严格 C11 O0/O2、JIT、module
和独立 AOT，以及两个／六个返回值、false／整数零／char NUL、bytes、
内嵌 NUL 文本、UTF-8、动态 null、赋值、丢弃字段和关键字参数。
不足／多余结果均报错并保留目标原值，原有 Stack 和综合动态回归通过。

五处动态 product 调用的 30 个 IR 变异在 C/header 输出前拒绝。两端
O0/O2 的完整同步 init/main/fini 分配前缀各扫描 47 次，首次不命中为
46，完整拥有账本平衡。实际生成程序验证合法零值、无诊断失败和
失败后清理；删除成功状态判断的真实 C 变异由故障宿主断言拒绝。
纯 typed 程序的 AOT 不包含新增 product 提供方。

原生专项两端各 27 项、四条闭合分配前缀和三份实际错误实现对照
通过，Linux 对所选实现启用 ASan/UBSan。对照分别检测静默丢弃
额外结果、忽略独立失败和因早期错误漏掉后续输入析构。

语言证据为 `E:/xlang-validation/stdlib53-b276-language-win-final1/results.json`
和 `/var/tmp/xlang-stdlib53-b276-language-linux-final1/results.json`；原生
证据为同批 `native-win-final1` 与 `native-linux-final1`。旧 TCC 只提供
真实 C 编译前置，不代表发布 CLI/DLL/VFS 已更新。

## 继续实施的范围

动态多返回值方法尚需完整向量 dispatcher，当前 lowering 明确拒绝
其未闭合的物理 ABI，不能退回单值或伪造 Array。下一步贯通 class
方法、Stack try fallback 和保留后的绑定方法，保留同名 class 方法
优先级，并复用统一 product 接收合同。

`xcall` 的 typed nullable 返回编解码仍需补齐；本批的 null 覆盖是
动态 `var` 字段，不能替代该类型化编解码验收。完整容器转换、Native
编码、工作线程／对象图故障、模块组合及发布总门仍继续。CRT 未决策
0、XRT 19 组、WaitQueue MPMC 及对象池最后的顺序不变。
