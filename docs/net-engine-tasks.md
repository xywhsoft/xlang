# NetEngine 的拥有型结果任务

`NetEngine` 的结果任务与 `post`／`after` 的脱离式回调分开：前者返回拥有结果的
`future<T>`，错误由该 Future 报告；后者仍用 Engine 的脱离式错误记录。两者
都直接运行在已选择的 Engine 工作线程上，不偷偷转投计算池。

## 公开接口

| 方法 | 额外参数 | 返回 |
| --- | --- | --- |
| `submit(work, affinity = 0)` | 无 | `future<T>` |
| `submitCancel(work, parent, affinity = 0)` | CancellationToken | `future<T>` |
| `submitAfter(delayMillis, work, affinity = 0)` | 非负毫秒延迟 | `future<T>` |
| `submitAfterCancel(delayMillis, work, parent, affinity = 0)` | 延迟与 CancellationToken | `future<T>` |
| `submitUntil(deadlineUs, work, affinity = 0)` | 原生单调时钟的绝对微秒期限 | `future<T>` |
| `submitUntilCancel(deadlineUs, work, parent, affinity = 0)` | 期限与 CancellationToken | `future<T>` |

`work` 是同步、无参数的类型化函数。`func()` 无返回值重载返回 `future<null>`，
不建立伪造的 null 结果槽；显式同步 `func() as null` 仍遵守现有语言限制。
不可复制的 generator 和多返回值不会由这套 API 获得特殊例外。`affinity` 是
原生 Engine 的工作者选择提示，不是系统线程句柄或互斥保证。

```xl
auto engine = NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto value = engine.submit(func() as string
    return "a\0你"
end)
assert (await value) == "a\0你"
engine.close()
assert engine.wait(10000)
```

`submitAfter` 拒绝负值和会溢出绝对微秒期限的有限毫秒值，不把溢出饱和成无限。
`submitUntil` 使用固定快照的 `uint64`／微秒／Until 合同；`0` 是已到期的绝对
时间，`UINT64_MAX` 保留原生 never 语义，只能由取消或关闭等事件结束。不要传
入墙上日期或独立 XRT 毫秒／For 时间。本批没有升级两边时间 ABI。

Engine 必须先 start，close 之后拒绝新任务。已有立即任务与 Timer 的停止／
取消／关闭结算复用原生 TaskNet；关闭未触发 Timer 不会等待到其原定期限。
取消不是强制中断已执行的任意代码；长回调应合作检查自己捕获的取消令牌。
不要在 Engine 工作线程执行长时间同步阻塞；需要这类工作时显式使用 TaskPool。

## 结果、生命周期与生成代码

每个调用使用编译器已经验证的 `XL6_FutureValueProfileV1` 与精确
`XL6_CheckedResultClosureBorrowV1`。驻留实现独立保留 Closure 和其 Invoke 适配
代码租约，两者可以属于不同模块；profile 按值复制，不保留模块临时描述地址。
结果直接写入已初始化的拥有槽，没有由 Cgen 生成第二套 T 析构或中转执行器。
标量、C 标量、char、time、string、bytes、类、容器、error、Closure、Future、
nullable 及现有受支持的原生句柄沿用同一结果模型。

`xl6_task_result_closure.c` 与原 TaskPool 共用闭包准备、结果槽和 checked 执行。
`xl6_task_net_language.c` 只负责物理 Engine 入口保护和提交适配，入口保护覆盖
失败回滚；`xl6_task_net.c` 复用共同激活记录与原生拥有型 Job／Timer。受理前的
失败不消费借用回调，body 失败和捕获清理失败保持既有优先级。可分配、提交或
报告错误的入口不标 pure／no-raise／nonblocking。Cgen 仅消费精确合同并发出
调用及拥有清理，没有添加符号名特例或实现函数体。

## TaskGroup 组合

`task_group_net` 用已经原子化的 `TaskGroup.start` 表达，而非增加平行的生成器：

```xl
import concurrent
auto group = TaskGroup(8)
auto work = group.start(func() as future<int>
    return engine.submitAfter(2, func() as int
        return 42
    end)
end)
assert group.joinTimeout(10000)
group.throwIfFailed()
assert (await work) == 42
```

`start` 先预留容量，再执行 starter，并返回原始拥有型 Future。starter 的提交
失败恢复容量并保留原错误；组取消传到原始 Future 与实际 Timer，不制造一个
已就绪的替代 Future。模块 checked 退役继续负责未显式 await 的 pending Timer。

## 验收边界

实现入口为 `check_stdlib_net_task_language.py`、
`check_stdlib_net_task_resident.py`；原计算池生产者回归为
`check_stdlib_task_submit_closure.py`。正式证据根为
`F:/xlang-validation/five-groups-20261023/p3-net-task-language`。
2026-10-23 正式验收 Windows 123／Linux 127，共 250 阶段通过：语言各 83，
网络驻留生命周期 20／22，原计算池生产者回归 20／22。源码分别重建 886／885
个 TU，TU 不混计阶段。正式源构建与语言轮次均为 `*-r2`，网络驻留为 `*-r4`，
计算池为 `*-r2`；输入、日志与产物 SHA 复核，原始 Linux 证据和 F 盘镜像保留。
准备／失败轮不计通过，提交和完整哈希见最终 `verified-commit.json`。

6 个本批语言用例、5 个通用泛型／旧 Engine／计算池回归覆盖 JIT、module 与
独立 AOT；本批用例另做严格 C O0／O2 编译和实际原生 selector 裁剪检查。
7 个静态负例及 173 项输出前坏合同拒绝保持精确 callback／result／profile／
error／borrow／执行 source 合同；不修改 verifier 使错误实现通过。

驻留选中闭包运行 GCC O0／O2、TCC、Linux ASan／UBSan。每种网络配置检查
72 个 body／cleanup／取消／立即与 Timer 场景、独立的 Closure／Invoke 代码
拥有者，以及 72 个 caller 分配预算（8 次拒绝、64 次受理），核对借用回调未被
错误消费和精确内存账本。旧计算池每种配置保留 144 个 checked outcome、
独立代码拥有者及已有 OOM 回归。另检查三种裁剪样例：普通 Engine 不带 TaskNet，
计算池不带 TaskNet，网络结果任务不带计算池／Executor。

本批修正一个通用语言推断问题：不含类型参数的具体／默认实参不参与 T 绑定，
其转换仍由普通语义及调用合同校验。否则合法整数默认值到 uint64 的转换会被
推断阶段误拒绝。独立正／负回归验证此规则，历史源码驱动器对同一正例的失败
反证另存，不冒充当前源码通过；没有添加标准库名字或 Cgen 绕行特例。

本项不关闭 NetFile、完整服务图／模块卸载矩阵、P4 密钥格式、X.509、TLS 或
shipping 联合验收。没有应用 Socket／网络数据／安全性质专项测试，没有更新
发布 DLL／SO／VFS。长期目标仍 active，未决策 CRT 0／XRT 11，对象池最后。
