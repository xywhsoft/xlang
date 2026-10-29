# 普通失败返回与外层回调的隔离

文本、Regex、Array、Dict 和队列五组操作的 worker 故障前缀已在新构建的
Windows／Linux 真实产品上通过。每端共享产品失败由 40 项降为 20 项，
独立 AOT 的原有两项失败消除。List、Set、Stack、Tree、SlotMap 五组仍有
错误传播缺陷，必需门禁保持失败，53 模块长期任务不结项。

## 返回合同与实现

后续五组和 List 读取链的修复及新产品结果见
[拥有值与槽复制的工作线程错误隔离](stdlib53-b329-owned-return-worker.md)。
本页保留本批原始范围与失败数字，不回写历史证据。

本批修复已有明确物理失败载体的通用边界：Array producer、浅拷贝／图复制、
拼接、投影、Stack 批量提取和原生句柄工厂。内部清理事务仍锁存首错、完整
执行销毁；到普通返回边界只交付拥有诊断，返回 NULL 或显式失败状态，
不提前把语言调用者的外层回调标记失败。

List／Set／Dict、Tree、队列的生成工厂，以及队列／Channel 状态返回，
按已有 IR 的 NULL／STATUS_OUTPUT 合同交付同样结果。没有加入库算法、
符号名猜测或新的 Cgen 特例，没有改变函数签名或增加公开 API。
`xl_foreign_error.h` 明确区分这种交付与 void 回调传播；仅靠诊断槽判断
失败的接口不能直接套用 NULL／状态返回的规则，仍需继续审计。

`xl6ErrorScopePropagateFailure` 本身未修改，Thread 也未忽略执行／捕获清理
作用域的失败。真实逃逸的 finalizer／回调仍锁存首错，包括 NULL 诊断。

## 双平台故障复测

生产源码来自 HEAD 加本批精确选择的 12 个实现／合同文件，不纳入其他工作区
修改；两端分别重新构建 TCC、XRT、VFS、CLI 和两份共享产品，不复用旧对象。
使用 B327 的十组真实语言 worker，不放宽任何 catch、finally、退出码或账本断言。

每端两个共享产品 × O0／O2 宿主的结果一致：

| 工作负载 | 每个宿主的连续故障点数 | 状态 |
| --- | --- | --- |
| 文本 | 34 | 完整前缀与下一未触发点通过 |
| Regex | 94 | 完整前缀与下一未触发点通过 |
| Array | 41 | 完整前缀与下一未触发点通过 |
| Dict | 55 | 完整前缀与下一未触发点通过 |
| MPMC／WaitQueue | 34 | 完整前缀与下一未触发点通过 |
| List、Set、Stack、Tree、SlotMap | 在部分前缀中止 | 20 项失败仍保留 |

通过项逐次检查拥有者堆账本、调用线程隔离与保留引用的 XRT 完成；这不是
OS join、运行库卸载或调度器全局故障证明。原生宿主 O0／O2 不代表 TCC
JIT 有对应优化模式。生成 C 的两种严格编译、独立 AOT 构建、十组各 budget 0
与无故障对照的 20 项运行，两端均通过。

新增 `stdlib_checked_return_scope_host.c`，每个产品／优化等级检查 400 次
producer NULL／非 NULL 诊断返回、200 次真实句柄分配失败回滚，以及干净／
已失败调用者的状态隔离。单独的真正逃逸 NULL 回调仍必须失败。
旧产品的四个宿主都被该测试的外层状态断言拒绝；不是靠超时或编译失败拒绝。
既有错误作用域测试的首错优先级、原始错误身份及平衡账本也在四个宿主通过。

`check_stdlib_worker_fault_shipping.py` 将这八项原生边界控制纳入必需检查。
每端共 84 阶段：`CheckedReturnIsolationAcceptance` 和
`CallbackFirstErrorAcceptance` 为 true，但 `Complete` 与
`WorkerBodyPrefixAcceptance` 仍为 false；剩余 20 项不是 expected 或跳过。

## 整批回归

每端的 312 个候选均完成清单检测。JIT／模块通道保留 303 项普通通过、
1 项预期拒绝、7 项专用宿主和 1 项平台条件；独立 AOT 保留 302 项通过、
1 项预期拒绝、8 项专用宿主和 1 项平台条件。通过的独立程序已迁移位置
并检查不依赖 xlang 共享产品；专用宿主和平台条件没有改成普通通过。

产品总门 Windows 630 阶段、11 个子门，Linux 631 阶段、12 个子门；
除必需的 worker 故障门外，其余子门全部通过。容器图的 204 个模式、
clear／析构、线程、同步、独立 AOT 图退役、终端就绪和完整资源账本没有新增
失败。两端总门仍返回失败，`Full53Acceptance=false`，不提升为发布通过。

## 剩余路径

真实 Linux 编译库的调试栈分别定位到 `xl6ListValueCloneV1`、
`xl6SetValueAlgebraV1`、Stack 批量插入中的字符串槽复制、`xl6TreeQueryV2`、
`xl6SlotMapCreateTakeV1`。它们仍通过内部通用 finish 或槽回调把普通失败
传播到调用者外层。后续应按 checked 返回、无状态载体回调和 void 清理的
角色完成共同边界审计，不按这五个测试名增加后端补丁。

## 证据

- 普通构建：`D:/xlang-validation/stdlib53-b328-build-win/build.shipping.json`、
  `/var/tmp/xlang-stdlib53-b328-build-linux/build.shipping.json`。
- 故障门：`D:/xlang-validation/stdlib53-b328-worker-win/results.json`、
  `/var/tmp/xlang-stdlib53-b328-worker-linux/results.json`。
- 产品总门：`D:/xlang-validation/stdlib53-b328-products-win/results.json`、
  `/var/tmp/xlang-stdlib53-b328-products-linux/results.json`。
- 独立 AOT 清单：`D:/xlang-validation/stdlib53-b328-aot-win/results.json`、
  `/var/tmp/xlang-stdlib53-b328-aot-linux/results.json`。
- 旧产品拒绝：`D:/xlang-validation/stdlib53-b328-old-control3-win/results.json`、
  `/var/tmp/xlang-stdlib53-b328-old-control3-linux/results.json`。
- 剩余栈：`/var/tmp/xlang-stdlib53-b328-trace/`。
- 输入与提交选择：`D:/xlang-validation/stdlib53-b328-dev/`。

CRT 未决策 0；XRT 19 组未决策、signal 注册线程显式泵、WaitQueue MPMC
和对象池最后的顺序不变。完整函数／分支、故障和发布验收继续。
