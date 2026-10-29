# B134：并发队列拥有值跨三次真实 await 验收

本批不增加标准库功能，不修改生成代码或运行库。它补齐 B133 留下的 Queue
continuation 验收，并保留 53 模块长期任务的原范围。

## 真实语言场景

`stdlib_concurrent_queue_continuation.xl` 使用三个原生队列：

- SPSC 产生含 NUL、UTF-8 的拥有式 string。
- MPSC 批量产生任意 bytes，并返回拥有式 `array<bytes>`。
- MPMC 产生捕获变量的拥有式 callable。

这些值跨越三个实际 Future await。bytes 的局部修改不能改变 Array 中原值；显式
写回后，新值继续跨 await 保持完整。测试没有把嵌套 indexed write 改写成编译器
支持的能力：当前语言场景明确提取 bytes 副本、修改，再写回 Array。

第三个 Future 分别成功、拒绝、取消、关闭。每种结果都检查语言 finally 在模块
fini 前恰好执行一次。宿主不替换 Queue、Array 或 closure 实现：只控制真实 Promise
在完成态、Watch 注册后、Commit 交接后这三个时点完成。挂起路径核对三个实际
Watch 注册，Commit 路径不再读取可能已经退休的 ticket。

共享 multi-await 测试支持只增加一个条件编译的 after-init 观察钩子。没有定义该
钩子的旧三次 await fixture 已用 O2、真实 pending-after-Commit 成功和取消路径复测。

## 当前证据

以下三个报告均为 `Complete=true`、`InputsUnchanged=true`，没有失败模式；结束后
再次逐项复核当前输入 SHA-256，没有变化。

| 门禁 | 报告（相对 `E:\xlang-validation`） | 阶段 / 模式 |
| --- | --- | --- |
| Windows 当前源码 resident 对象 | `stdlib53-b134-continuation-win1/continuations.json` | 51 / 24 |
| Linux 当前源码共享运行库 | `stdlib53-b134-continuation-linux1/continuations.json` | 51 / 24 |
| Linux 生成代码与测试宿主 ASan/UBSan | `stdlib53-b134-continuation-linux-san1/continuations.json` | 51 / 24 |

每份报告覆盖 O0/O2 × 四种终态 × 三种完成时序，每个模式运行两次完整 module
init/fini；合计 72 模式、144 次语言生命周期。逐次验证结果／错误种类与 payload、
finally 计数、实际 executor 清空，以及真实分配／释放／存活字节平衡和非法释放、
重复释放、释放后使用计数。C 生成重复结果一致；lowered IR 具有一个已解析的 async
machine、三个 suspension、13 个 machine entry 和明确 Future outcome ABI。

可复用入口：`tools/check_stdlib_queue_continuations.py`。Windows 需要经过哈希校验的
当前源码 public build 目录；Linux 使用当前源码产品。运行时原生队列的调用必须
存在，生成端旧 `xlir_special_queue_` 实现必须不存在。

## 范围边界与后续

- Linux sanitizer 只覆盖生成 TU 与测试宿主；共享运行库未插桩，报告明确
  `RuntimeInstrumented=false`。内存账本检查不替代全运行库 sanitizer。
- Windows 使用已验证的 B133 源码对象；本批没有重建所有发布 DLL/VFS。
- 本批不是公共 API/JIT 模块退休、调度器全线程 OOM 或 53 模块发布级总验收。
- AvlTree 完整 resident owner、错误／相等性合同、对象图接入及旧总门禁整理仍待完成。
- 不扩大未批准功能；对象池仍留在最后。
