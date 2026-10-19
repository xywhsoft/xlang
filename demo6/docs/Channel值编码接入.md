# Channel 同步值编码：闭包与 union

本批完成同步 Channel 的闭包和 union 消息接入，不等于全部 Channel 或 P1
完成。Case／Selection 和 Future 的编码接入仍须沿用下述闭合合同继续开发。

## 语言值和存储槽分开证明

`XL6_IrChannelValuePlan` 是共用 authority：记录 schema、逻辑元素／基础类型、
codec、物理槽、复制协议、可空与逻辑 tag。它从 canonical 类型和完整 Array
schema 重新证明，不以指针宽度、诊断名称或调用者变量猜测表示。

- DIRECT 保留现有标量、string、bytes、对象、Future 和驻留记录的槽合同。
- Closure 保留语言的闭包指针表示；传输存储是拥有的 callable xvalue。
  boxed callable 拥有原闭包及编码入口租约；接收投影另外拥有 boxed value 和
  接收入口租约，不借用生产者栈或 Channel 槽。
- Union 是已验证、无托管引用的 C union。传输是精确名义类型及大小的字节值，
  不发明 active member、相等／哈希规则；修改源 union 不改变已发送消息。
- 可空消息的成功 null 由 status 与值共同表示，与空队列、超时或关闭区分。

原生仍复用现有私有消息盒、拥有策略和图边；没有新增传输实现或 XRT 算法。
Cgen 只消费闭合计划，输出既有 box／project 调用、原生调用与清理；没有生成
Channel 队列、分配、同步或图遍历算法。编码失败不得把空物理指针当作成功的
nullable 消息传入原生入口，结果投影失败必须保持 checked 错误和拥有清理。

传输闭包只保证自身及捕获的生命周期，不会为用户可变捕获自动加锁；跨线程
共享状态仍遵循已有同步／线程亲和合同。

## 同时修正的共用 IR 缺口

可空拥有结果合同补齐合法 union，仍验证精确基础类型，不接受同大小异类型。
泛型 Channel 的 close／count／isClosed 明确登记 dependent 合同；具体特化后
必须重新取得 VERIFIED 槽证明。close 的语言返回值继续为 void；原生 bool 是
错误载体，不是新增语言返回值。未实例化的泛型不能借机获得物理槽许可。

旧“unsupported closure”recipe 已改为真实正例，并增加仍无消息槽的 generator
待定例。独立 C 工厂使用物理 xvalue 槽，不把语言 closure 指针伪装为相同宽度
的存储槽；实际闭包内容、执行与清理另由语言正例证明。

## 验证与边界

证据根：`F:/xlang-validation/five-groups-20261020/p1-channel-codecs/`。

- 完整源码：`source-win-r2`／`source-linux-r2`，624／623 编译单元。
- 新专项：`tools/check_stdlib_channel_codecs.py`。严格 C O0/O2、真实 JIT／
  module／独立 AOT、14 个闭合域、126 个 schema 及 48 个泛型控制变体；每种
  优化各 348 次 C/header 零输出拒绝。覆盖七种收发模式、null、队满、期限、
  取消、关闭、未接收消息的析构错误、字符串 NUL、void/C 标量及回调异常。
- 分配失败：实际逻辑分配预算，分别扫过闭包 5 个、union 2 个命中前缀及随后
  成功；检查失败无消息发布、无 null 回退，并保留原输入。只使用当前线程的
  一次性预算，不在语言执行中切换全局内存调试开关。
- 真实生产线程退出后接收、调用带 NUL 字符串捕获的闭包，并在关闭 Channel
  后继续使用结果；不由同线程模拟代替。
- 两端 `recipes-*-r1` 保留旧接收／队列 IR、真实 schema 工厂、普通语言回归；
  `native-*-r3` 保留真实 Future／驻留记录消息、分配前缀和精确拥有账本。

最终冻结报告为 `language-win-r3`／`language-linux-r2` 各 86 阶段、
`recipes-win-r1`／`recipes-linux-r1` 各 20 阶段、`native-win-r3`／
`native-linux-r3` 各 4 阶段，共 220 阶段通过。提交前逐项复核完整源码
输入、对象、编译器、日志和产物哈希；零测试报告或工具变更前的旧报告不计入。

开发期 `pilot-*` 只用于快速定位，不是正式证据。失败轮次不覆盖；正式报告
必须完成、输入未变且日志／产物复核一致。没有网络、扫描或安全专项，也不
替换 release、推送或宣称 fresh shipping DLL/SO/VFS 已通过。

## 下一步，不能假装已经完成

V1 generic native 调用仍直接接收语言槽；其 Channel schema 边界继续只接受
DIRECT。新增 boxed codec 不是把这些槽强制转换为 xvalue 的许可。下一批须将
参数、返回的物理编码与逻辑投影纳入共用 native-call 计划，再接入同步选择、
有限／无限期 Future 及动态方法适配，不能按库名另写转接实现。

本批另记录了实测基线缺陷：`if optionalUnion != null` 后直接读 union 字段曾报
“dynamic member expression has no resolved object result ABI”。旧编译器也失败，
证据为 `nullable-union-member-before.log`；本批通过已支持的 `??` 取得值验证
消息内容，当时未把该成员访问问题标成解决。后续独立共用层修复与证据见
[Nullable union 字段投影](NullableUnion字段投影.md)，不归入本同步编码批次。
