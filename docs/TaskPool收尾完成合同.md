# TaskPool 收尾与完成观察合同

本批修复共用驻留生命周期，不改变语言语法或原生 XRT 算法，也不放宽拥有图读取。
Cgen 的模块准备路径只输出 `xl6TaskPoolModulePrepareV1` 调用，不自行判断 join、
重试、拥有权或已完成状态；声明、JIT 符号及 AOT 裁剪登记同步。

## 已启动收尾不能丢失

此前一次非阻塞 teardown 可以启动 native stop/join 并返回 BUSY。后续模块 preflight
却跳过这项未完成职责；native Count／Trace 正确返回 BUSY，导致拥有图尚不能创建，
也就不能用图内 preparation 推进这个 join。

现在 wrapper 用受相同 gate／mutation 保护的 `quiescencePending` 记录实际已请求的
职责。普通 preflight 不发起新 stop，只续跑已请求的职责；未完成时仍返回 BUSY。
它在 Freeze 外执行，不用暂停、推算线程完成、跳过信用或放宽 Count／Trace 绕行。
明确 teardown 仍选择原来的阻塞／非阻塞模式；失败的 checked 状态与错误合同不变。

## Commit 后只交还真实拥有引用

独立 AOT 还暴露了已有的 parent-before-child Finish 边界：图已经证明全部 preparation
并 Commit，模块却再次调用普通 native 控制。cleared native 节点不允许这些调用，
而普通池 Destroy 也不应抢先消耗图所管理的 outgoing native／exit 拥有引用。

共用模块准备函数识别真实 claim＋cleared 状态，不重复 preparation。统一的 owner
release 在此状态只交还模块那份引用，保留真实 plan pin 和 outgoing owners，交由
共享图 Finish 处理。普通 unclaimed Destroy 仍要求 native 已移除；显式 cleared-only
入口继续严格拒绝其他状态并保留原 StateError。没有新增假引用计数或特殊清理旁路。

## 已观察的完成是单调事实

原生零超时 TryJoin 是排他就绪探测；无关的拥有权 mutation 也可能让它返回 TIMEOUT。
但语言 `wait` 已经确认真实 worker／ThreadKey 尾部完成后，重复等待不应因此退回
“未完成”，也不应让随后取消覆盖已确认完成。

只有完整 native join 返回 OK，wrapper 才发布 `exitComplete`。关闭的准入不能重启
该池；重复等待和已进入的并发等待共享这个事实。它只证明等待完成，不证明可以
Destroy、Count、Trace 或跳过其他在场控制。未完成的取消／超时、legacy drain-only
Wait、自身等待拒绝、checked 首错与失败计数都保持原合同。

## 验证

入口为 `tools/check_task_pool_completion.py --source-build <build.json> --out <fresh-directory>`。
使用完整当前源码编译器，核对输入、日志及产物哈希：

- 三份真实语言用例覆盖固定就绪队列、物理 worker exit 与 ThreadLocal 清理错误，
  分别经过严格 C11 O0／O2、JIT、模块及独立 AOT；AOT 不选择 XRT ALL。
- 31 份 TaskPool、Executor、Promise、Channel、取消及 null 表达式回归；三类未观察
  失败的 JIT／模块运行必须 checked 失败，不能用 abort 或编译失败冒充。
- O0／O2 各验证 32 次 pending stop 的 preflight 续跑、真实 ThreadKey 屏障、
  BUSY Count 不发布输出、6144 次无关 mutation 并发期间的已完成等待和精确内存账本。
- 原有九个模块 ABI 分区和 114 个错误 IR 变体仍在两种优化下验证。
- 两种旧行为分别先编译成功，再由真实收尾／完成断言拒绝：丢弃 pending preflight
  职责，以及不保留已确认的物理完成。不能把编译失败当作有效负控制。

这不是 shipping DLL／SO／VFS 或完整 P1／五组验收；不含网络或安全性质专项。

## 当前源码验收记录

2026-10-20，最终完整源码驱动为 Windows 624／Linux 623 单元。
`source-*-r3` 与 `language-*-r2` 为最终语言证据，两端各 104 阶段；旧控制矩阵
`control-win-r1`／`control-linux-r2` 为 61／63 阶段。四份报告共 332 阶段、
18 项预期拒绝，Complete／InputsUnchanged／LogsUnchanged 为 true，无失败。
输入、对象、日志与产物在提交前再次按哈希核对。

旧控制矩阵继续覆盖两套头、GCC／TCC、O0／O2、最小部署、原生 ThreadKey
屏障及分配前缀；每个旧图 profile 保留 280 次平衡生命周期、240 个 queued
拥有图、520 次实际准备和两种 Finish 顺序。Linux 选定控制闭包另通过
ASan／UBSan／泄漏检查，不冒称整个语言运行时都已 instrument。

证据目录：`F:/xlang-validation/five-groups-20261020/p1-task-pool-completion/`。
早期 AOT 失败、诊断用对象替换及输入变化的源码轮次只作发现材料，不计入验收。
没有替换 shipping 文件、同步 XRT 快照或推送；其余长期任务范围不变。
