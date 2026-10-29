# Nullable callable 的存在性与载荷合同

2026-10-14。typed func 与 xcall 之间的基础 nullable 参数和返回值
使用统一 IR 的明确存在性计划。null 与 false、零值、空字符串和
空 bytes 区分；基础载荷继续采用原来的类型身份、范围和拥有规则。

## 存在性与载荷

每个桥接参数和结果记录物理类型、载荷类型、存在性 ABI、编解码
opcode、整数域／C 标量 profile，以及调用和清理方式。验证器重新
建立规范计划；存在性或载荷字段被破坏时，在 C 和头文件输出前拒绝。
Cgen 执行计划，不从聚合体零值判断是否存在。

普通标量使用 `hasValue`；string 以 Data 为缺省哨兵；bytes 使用
XValue null 哨兵，不把未初始化的 C NULL 当成合法 nullable bytes。
参数解码成功后才设置存在性，反向调用在全部转换和清理完成后提交
返回值。现有 checked 状态、首个失败保存和拥有结果事务保持不变。

基础范围包括 bool、char、全部八种整数宽度、float32／float64、time、
数字枚举、21 种 C 标量／type profile、string 和 bytes。该范围
不等于所有 Native class、Future 或 nullable callable 嵌套已支持。

## 默认值与参数副本

默认值的分类由统一 IR 提供，lowering 复用同一分类；移除了一份
重复分类实现。NULL 默认值以及既有可认证的非空字面量默认值进入
相同计划，关键字参数沿用闭合的参数绑定规则。

修改参数的函数仍消费独立拥有值。nullable string／bytes 的调用
不会让源变量随参数替换而变化；只读借用与拥有输入按既有参数 ABI
执行，内部没有新增语言 byref／byval。

## 验收范围

Windows 全源码 333 个翻译单元构建和 234 项语言检查通过。检查
包含 19 个程序的重复 C、O0／O2 严格 C、JIT、模块及 AOT；旧标量
callable 的宿主明确执行 main，不以只完成模块初始化代替功能运行。
两份 nullable IR 合同分别拒绝 110 和 330 次实际描述破坏。

六个程序在两个优化级别闭合 12 条 OOM 前缀。新增基础 nullable、
完整标量域和修改参数用例的首次未命中分配点为 109、98、55。
实际生成 C 的存在性丢失反例被运行测试拒绝；未使用的 nullable
签名不带出整数／F32 callable 编解码提供方。

Linux 全源码 332 个翻译单元构建和完整 234 项回归通过。旧 Stack
O2 插桩编译触发初始限时后，在相同输入下续跑剩余 24 项并通过；
原始 210 项结果保持不变。插桩对象是生成的严格 C 与宿主，
不是链接的编译器／XRT 共享提供方；JIT 和 AOT 不因此宣称已插桩。
语言证据为 `E:/xlang-validation/stdlib53-b278-language-win-final1/results.json`
与 `/var/tmp/xlang-stdlib53-b278-language-linux-final2/results.json`，
后者绑定原始 partial 报告、已完成产物和续跑结果。

## 仍需完成

`float32(var)` 仍缺少规范转换目标，动态 callable 的直接 float32
表达式参数也未获准装箱。两项属于语言核心问题，已保留
`stdlib_dynamic_f32_conversion.pending.xl`，不能用预先赋给 var 的
写法替代最终修复。nullable 编解码专项已覆盖 typed float32 往返，
但不意味着这些普通动态转换已完成。

完整容器转换、动态／Native 编码组合、工作线程与对象图故障、模块
组合及 CLI／DLL／VFS 发布总门仍继续。CRT 未决策 0、XRT 19 组、
WaitQueue MPMC 和对象池最后的顺序保持不变。
