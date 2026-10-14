# 动态方法多返回值与 Stack 安全接收

2026-10-14。动态类方法、保留后的绑定方法和动态 Stack try 已贯通
统一 IR 与完整原生返回向量。多值接收严格检查返回数量，失败不更新
目标变量；空容器与取到可空元素的 null 保持不同含义。

## 统一 IR 与返回向量

方法声明本身建立规范返回 product，不依赖是否出现静态调用或函数值。
动态方法登记和验证选择同一类型，每个可适配字段都有明确的装箱与
拥有规则。接收上下文给出结果数量，结果是拥有的 `tuple<var, ...>`，
不是 Array，也不按方法名称推测字段类型。

立即调用和绑定调用共用完整向量 dispatcher。绑定 callable 保留
接收者及原生代码生命周期，并保存全部返回值；只有最外层普通单值
表达式投影第一项。多值接收数量不足或超出均报 TypeError，在目标
赋值之前失败。类方法内部副作用不因随后数量不匹配而回滚。

适配器在进入目标之前完成参数投影，完整消费输入并处理尚未装箱的
返回后缀。checked 调用使用独立成功状态，零值和 null 错误对象都不
代替成功判定。共享原生提供方完成保留、结果清理和事务发布；这些
内部输出指针不恢复语言 byref 语法，也不改变 string／bytes 值语义。

## Stack try 的动态合同

`tryPop`、`tryPeek` 和 `tryPeekAt` 使用明确 opcode 和已验证的 Stack
角色，不把任意 HANDLE 转成 Stack 指针。原生接收者认证和投影仍由
驻留序列层承担；同名用户类方法优先于原生 fallback。

`auto found, item = stack.tryPop()` 在空栈时返回 `(false, null)`；
可空栈中存在 null 时返回 `(true, null)`。`tryPeekAt` 的负深度为
RangeError，非有符号整数参数为 TypeError，超出栈大小则正常返回
未找到。失败数量不改动接收变量，但已经完成的 pop 不因调用之后
的清理失败而回滚。

`var take = stack.tryPop` 在没有任何同名用户类时也建立拥有的绑定
callable。释放原始接收变量后仍可调用；非 Stack 容器不会获准使用
该原生操作。旧 pending 用例已进入实际语言验证，文件名保留为历史
证据路径，不再表示该用例待实现。

## 验收结果

Windows 333、Linux 332 个翻译单元的完整源码严格构建通过。双端各
135 项语言检查覆盖 11 个用例，包括重复 C 输出、O0／O2 严格 C、
JIT、模块和按需 AOT。三个用例在两个优化级别分别闭合 OOM 前缀，
首次未命中分配点为动态方法 88、独立 Stack try 22、动态函数 46。

IR 对照检测 12 个动态 product 的 72 次破坏、两个原生绑定的六次
破坏、三个方法返回描述的九次破坏；全部在 C 和头文件输出前拒绝。
状态边反例分别证明动态函数和方法不能忽略无错误对象的失败。

原生 Stack 专项双端各 87 项、18 条闭合分配前缀和 11 份实际错误
实现对照通过；原生返回向量专项双端各 27 项、四条前缀和三份对照
通过。Linux 对所选实现开启 ASan／UBSan。生成 C 的附加对照验证
绑定向量截断和原生操作抢占类方法均会被实际运行测试拒绝。

语言证据为 `E:/xlang-validation/stdlib53-b277-language-win-final1/results.json`
和 `/var/tmp/xlang-stdlib53-b277-language-linux-final1/results.json`；
原生 Stack 为同批 `stack-native-win-final3`／`stack-native-linux-final1`，
返回向量为 `native-win-final2`／`native-linux-final1`。附加生成 C
对照为 `bound-win-final3`／`bound-linux-final1`。

## 后续工作

本批未扩宽仍不支持的 typed nullable callable 编解码。动态字段与
原生类型的完整组合、容器转换、工作线程／对象图 OOM、模块组合及
CLI／DLL／VFS 发布总门仍继续；专项通过不等于 53 个模块总体完成。
旧动态 TCC 核心仅作为真实 C 编译前置，不代表发布产物已更新。

WaitQueue 采用 MPMC，支持多个消费者，继续保留 `(status, T?)` 拥有
接收接口。CRT 未决策 0、XRT 19 组及对象池最后的顺序不变。
