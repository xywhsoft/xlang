# 受管理类签名与真实产品补测

2026-10-14。三组标准库的 30 个受管理类经实际标准库展开取得完整
声明，329 个公开可调用签名均找到已解析源调用。Windows 和 Linux
各扫描 307 份夹具，已有 228 个命名空间签名仍完整。四份新增补测
双端通过真实 run/module、严格 C 及独立 AOT。本批不修改生产实现
或扩张公开 API，内建容器与动态派发的完整覆盖仍需继续。

## 声明清单和绑定规则

`stdlib_class_catalog.xl` 只用于实际 expander 的声明展开，不执行其中
的默认构造。File/IO 类型与流方法选择已批准的进程扩展，不手工拼接
分散的 class 声明片段。输出包括方法、get/let、构造器、私有成员、
表示字段和自动析构；筛选范围固定为三组的 30 个受管理类。

| 声明类别 | 数量 | 验收方式 |
| --- | --- | --- |
| 公开可调用签名 | 329 | 已解析源调用，缺失为零 |
| 普通公开字段 | 4 | 单独登记，不伪装为方法调用覆盖 |
| 内部组成构造 | 3 | 参数为内部 Pattern 载体，保留实现合同 |
| 自动析构 | 24 | 归生命周期与故障门，不要求源显式调用 |
| 私有成员 | 21 | 不加入公开 API 分母 |
| 原生表示字段 | 24 | 保留布局合同，不计普通方法 |

构造器与泛型类实例具有独立符号。测试宿主使用源位置、实际名义类型
及完整 IR 参数向量校对，不把 helper 中的另一重载当作源调用。
调用 ABI 的隐式 receiver 与构造器的真正参数分别处理；静态方法不
移除第一个公开参数。FixedStack 的具体实例继续要求类型参数一致。

新增反例在无关名义类上调用同名 `len()`，不能覆盖 Buffer.len；只
构造 `Buffer(string)` 时，其 bytes/point 构造器也不能计为已调用。
上一批的命名空间重载、泛型和注释／文字假调用反例继续通过。

## 新增实际补测

- 文本与正则：显式迭代协议、直接快照构造、RegexReplacer 回调和内嵌
  NUL 结果，FixedStack 的可空 peek 与独立 copy/clone。
- Buffer：新分配原生 handle 的一次性接管、原始字节指针调用、索引
  协议、比较和迭代。C sidecar 调用真实提供方创建新资源，不接管
  借来的语言 Buffer handle，不替换任何提供方。
- signal：注册线程检查、事件字段及直接 SignalHandle 构造与退役。
  raise 仅作用于本次受管理程序，结束时 reset。
- process：直接构造、stderr 与持续时间、阶段状态、File 重定向、
  stdin 关闭、温和／强制／树终止和受限 interrupt。只操作独立受控
  子进程；finally 负责强制清理和等待，不影响用户进程。

温和终止是请求，不保证任意子进程立即退出。Windows 初期失败来自
睡眠子进程不配合 Ctrl+Break 的测试假设；最终用例以 echo 子进程配合
stdin 关闭，仍验证 terminate 的请求和实际退出。Windows 无独立进程
组的 interrupt checked 拒绝与 POSIX 的定向中断分开验证，不广播
控制台信号。文本初期的保留字与 nullable 比较错误亦已修正；旧失败
日志保留，不作为生产实现缺陷报告，也不改写为通过。

## 结果与证据

两端静态门各 311 阶段；四份用例真实产品 run/module 共 16 阶段，
独立 AOT 构建／执行共 16 阶段，两端严格 C 各 8 阶段。源码批量
执行工具的 C sidecar 接线额外双端通过；产品父门也登记该 sidecar，
以便后续完整批量验收，不把新增 extern 留成未解析符号。

证据为 `E:/xlang-validation/stdlib53-b267-bindings-win-final2`、
`/var/tmp/xlang-stdlib53-b267-bindings-linux-final2`，严格 C 对应
`stdlib53-b267-strict-win-final1` 与 Linux 同名目录；实际产品执行为
`E:/xlang-validation/stdlib53-b267-dev/surface-runtime4.json` 和
`surface-aot1.json`。源码 sidecar 集成为 `stdlib53-b267-sidecar-win-final3`
及 `/var/tmp/xlang-stdlib53-b267-sidecar-linux-final2`。

这些测试复用经输入核对的完整核心和上一批真实 CLI/库/VFS 产品，
生产源码没有变化。Buffer 的显式 C sidecar 会选择原生完整需求，
不作为按需裁剪的证明。329 个静态签名也不证明所有运行时分支、
动态调用或所有泛型组合；ClassCompletenessAcceptance 与
FunctionCoverageAcceptance 仍为 false。

完整内建容器清单、字段与动态调用、跨线程／对象图 OOM、生成模块
组合及发布总门继续。CRT 无未决策，XRT 19 组、WaitQueue MPMC 和
对象池最后不变。
