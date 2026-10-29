# 拥有型原生记录的声明式调用

声明式外部调用现在可借用闭合的拥有型原生记录参数，并返回独立拥有的记录。
SlotMapKey 的比较和复制使用同一份原生调用注册表；Cgen 不增加函数名分支，
调用参数不经过 Value 装箱或额外的拥有权复制。这是 SlotMap 公开接线所需的
调用能力，不代表其泛型构造、完整方法和成对迭代已经完成。

## 声明和生成代码

原生调用登记位于 `src/abi/xl6_native_value_contracts.inc`，类型布局、Copy、Drop
和语言身份仍来自已有的闭合 native value profile。语义层只允许登记的符号，
XLIR 验证记录 ABI、借用参数、拥有结果和效果合同。未知的普通 C extern
不能因此接收拥有型记录；记录也不能降为 POD、裸指针或任意外部结构。

```xlang
extern "xl6SlotMapKeyEqualV1" @pure @no_raise @nonblocking func nativeEqual(SlotMapKey left, SlotMapKey right) as bool
extern "xl6SlotMapKeyCloneV1" @pure @nonblocking func nativeClone(SlotMapKey key) as SlotMapKey
```

实际生成 C 的调用是 `v44 = xl6SlotMapKeyEqualV1(v42, v43);` 和
`v51 = xl6SlotMapKeyCloneV1(v50);`。共享 ABI 头声明记录和函数，生成代码不重复
定义结构体。比较不需要错误作用域；复制仍保留显式 foreign error scope，
不能依据空键猜测调用成败。nonblocking 不表示无锁保证。

## 拥有权和错误

参数按 C 结构体值传递只搬运载体，不转移或取得引用。返回记录取得自己的
实例引用，原键和原表释放后仍有效；键不保活原表、元素或生成模块。
复制复用 checked Copy 边界，失败返回空记录并保留可检查的失败状态。

原生函数若已返回拥有值又报告失败，语言错误路径先接管再释放该值。
失败位独立于诊断对象；有诊断和没有诊断的失败都不能误判为成功。
checked 脚本入口保留原生报告的诊断，宿主检查后清除；无诊断失败仍保持
无诊断。每个失败前缀之后完整重试，不改变调用方拥有的键。

## 验证和剩余工作

Windows 和 Linux 的源码专项各通过 79 阶段、54 次完整失败前缀、276 项损坏 IR
检查和三个实际编译的坏实现检查。真实 JIT 各执行 100 轮有效键调用，严格 C
O0 和 O2 各执行 1000 轮。参数进入语言程序前原表已释放，错误路径覆盖
返回拥有值后的两种失败形式。

原生专项各通过 77 阶段、86 次分配失败前缀和五个坏实现检查；Linux 对这个
选定原生闭包使用 ASan 和 UBSan，不宣称完整 JIT 运行时已通过 sanitizer。
Array、List、Set、Dict 精确值域和可变 Set 键身份规则的语言回归通过。
组件检查通过 263 个组件、1396 个准确导出、69169 对组件闭包和 13 个拒绝控制。

证据位于 `E:/xlang-validation/stdlib53-b213-*` 和 Linux 的
`/var/tmp/xlang-stdlib53-b213-*`。这不是发布 DLL、SO 或整组 53 模块验收。
SlotMap 泛型工厂、完整方法、成对迭代、frame 和 cell 消费边界继续；对象池最后，
CRT 未决策 0、XRT 未决策 19 组不变。供应方 XRT 没有修改。
