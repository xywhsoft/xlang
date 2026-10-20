# Channel 原生调用的逻辑值与存储槽

Case／Selection、直接和有限期 Future 使用共享 `XL6_IrNativeCallValuePlan`。
登记表给出输入参数和结果方向，canonical Channel／Sequence 计划给出逻辑元素、
基础类型、codec、物理槽、拥有复制协议及可空表示。普通调用返回空计划；
不以指针大小、函数名片段、调用者变量或 Cgen 回溯源语法推断编码。

## 表示与拥有边界

- DIRECT 保持既有语言槽合同，不为普通 Promise 或其他外部调用强行装箱。
- 闭包／union 的消息存储是拥有的 xvalue 槽；Case、Future builder 只在编码
  成功后接收该槽的临时地址，原生策略独立复制。地址本身不逃出调用。
- Selection.value 与 ChannelReceiveResult 的原生读先输出独立拥有的存储槽，
  成功后才投影语言闭包／union，再释放临时 carrier。可空成功值与失败独立。
- 描述符保留逻辑 T 的名义身份，但大小和复制种类描述**存储**：boxed 值使用
  `sizeof(xvalue*)`／XVALUE，不能将 Closure* 或 union 的语言地址直接传入。
- 直接调用和动态 native witness 共用同一准备／投影／清理 renderer；分配
  失败记录 checked outcome 并跳过原生进入，不能把失败 NULL 当作可空消息。
  捕获和生产／接收代码租约仍由已有 callable box／projection 负责。

原生运行时算法没有变更；没有向生成 C 添加 Channel 实现、图遍历、分配器、
调度器、回调框架或按库名判断的实现分支。

## 类型依赖闭合，不普遍制造数组

原生结果读可以只出现于一个泛型资源类中，本模块不必创建 channel<T>。
相应的值见证由已登记的 `sequenceValueResult` 声明及其精确具体类实例引入，
在类型／派生合同阶段闭合；Cgen 不临时 intern 类型。

发现轮次中，“为所有泛型类引入 Array<T>”过于宽泛，并造成 TaskPool 的
持久化合同回归。最终实现撤去该规则，仅引入登记声明实际需要的依赖。
后期 callable signature 闭合若扩展类型集合，同时刷新派生 persistence ABI，
不以跳过 verifier 或取消 TaskPool 回归来解决。

## 同时闭合的共用 async 事实

泛型 async 推断将 CALL 的 future<T> 与 worker 结果 T 分层比较；T? 的物理
ABI 随特化变化，不在未特化阶段要求指针／tagged 表示相同，子类型身份仍须
精确统一。

plain union 的 async 结果新增明确的 `COPY_UNION`。它是精确名义、无托管
引用的 union 值复制，复用驻留 trivial payload；不是 Channel 的 boxed 存储，
也不借用 COPY_TAGGED、XVALUE 或整数标签。worker、await、Future／Promise
均保留同一类型身份和独立值语义；校验器拒绝错误结果标签和其他名义 union。

## 验证入口与不可扩大宣称的边界

`tools/check_stdlib_channel_native_codecs.py` 使用冻结的完整源码编译器。
八个专项程序覆盖普通／可空值、NUL 捕获、同步及异步选择、直接／有限期
Future、期限／取消／关闭、结果独立拥有、纯 Future union、未选择资源的
sole-owner 拥有图、checked 析构失败和实际分配失败前缀。

动态测试通过独立 `var` 形参阻止静态化，并检查真实动态 witness 函数体包含
存储输出与 checked 投影。直接和动态结果投影分别命中 2／7 个分配前缀。
Case、直接 builder、有限期 builder 的输入前缀为闭包 15／18／19、union
12／15／16。无本地 Channel 构造的 read-only 类单独证明见证不靠偶然同模块
类型存活；纯 union Future 不选择 XRT Channel 实现。

每个程序包含严格生成 C O0/O2、真实 JIT、module、独立 AOT；另保留 14 组
常规回归和独立 C 合同的 O0/O2 拒绝测试。证据根为
`F:/xlang-validation/five-groups-20261020/p1-channel-native-codecs/`。
发现期 pilot、失败／输入变动轮次不算最终验收；不替换 release 或推送。

最终冻结报告为 `language-win-r3/results.json` 与
`language-linux-r2/results.json`：Windows／Linux 各 96 阶段，共 192 阶段通过，
输入与日志均未变化；完整源码构建分别为 624／623 单元。每端 C 合同在
O0、O2 各拒绝 138 个损坏的 canonical 事实，并完成 276 次 C/header 零输出
拒绝；包括其他名义 union 的 worker／await 结果，不仅比较同宽 ABI。
提交前复核源码、头、对象、驱动、链接响应、语言输入、日志和产物哈希。

本批不等于 P1 或五组目标完成。source message legality／复杂拥有图及完整
新产品联合验收仍开放。另保留一个旧源码同样失败的基线：flow-narrowed
nullable 实参进入推断式泛型 async 返回 T? 的投影在本批时尚未完整闭合，见外部
`nullable_inference_probe.xl`、`inference-before.log` 和 `inference-probe.log`。
本批采用显式类型实参验证 nullable 编码，不据此声称修复了泛型推断。
随后独立的 [泛型调用值推断合同](泛型调用值推断合同.md) 在共用 IR 层修复
该基线；不是 Channel 或 Cgen 特判，也不代表其余核心问题已经封板。

随后消息 source legality、嵌套 codec 和真实循环的独立实现／验收见
[Channel 消息准入与拥有图](Channel消息准入与拥有图.md)。不追溯扩大本批
192 阶段的范围，也不将普通作用域退出描述为 eager 循环回收。
