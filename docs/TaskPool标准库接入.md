# TaskPool 标准库接入

2026-10-20，五组长期任务 P1。同步闭包拥有结果、真实工作线程 checked 退出及
完成等待增量已接通；不据此关闭 P1、整个五组任务或发布验收。

## 公开接口

显式 `import concurrent`：

```xlang
auto pool = TaskPool(2, 1024)
auto result = pool.submit(func() as string
    return "a\0你"
end)
pool.close()
assert pool.wait(10000)
assert (await result) == "a\0你"
```

`TaskPool(threads = 0, queueLimit = 1024, stackBytes = 0)` 独立拥有自己的原生池；
0 线程数采用 XRT 自动配置，队列容量必须为正且是全池容量，不是每 worker 容量。
创建为 lazy，`start()` 可显式启动；读取 `snapshot()` 不启动工作线程。

`submit(func() as T)` 返回拥有 `future<T>`；`submit(func())` 返回无 payload 的
`future<null>`。`submitCancel(work, parent)` 将同一提交接到取消令牌。泛型 T
由回调签名推导，不引入 `pool.submit::<T>` 的新成员调用语法。

控制面为 `start/close/cancel/wait/waitUntil/waitCancel/waitUntilCancel/snapshot`。
等待前必须 close 或 cancel；相对等待 -1 无限、0 轮询、正数为毫秒，其他负数
拒绝；绝对截止时间为 pinned 微秒，MAX 保留值拒绝。成功等待 true、超时 false，
等待取消抛 CancelledError，操作错误仍 checked。`close()` 拒绝新任务但排空已受理
任务；`cancel()` 同时请求取消，不能强行终止正在运行的用户代码。

`TaskPoolSnapshot` 是独立不可变快照，公开 threads、queueLimit、queued、running、
submitted、completed、succeeded、failed、cancelled、rejected、initialized、
closed、cancelling。计数来自实际原生统计，不根据 Future 状态猜测。

body／捕获析构失败由各自 Future 报告，不计入退出失败。真实工作线程退出时的
ThreadLocal 清理另用共享的精简首错＋失败计数账本，由 `wait/throwIfFailed`
checked 报告；`exitFailureCount/exitFailureSaturated` 查询稳定历史。它不改变
Snapshot 中仅统计任务的 succeeded/failed，也不引入 detached Executor 调度。

语言 TaskPool 的成功等待包含已受理任务／捕获尾部、已注册异步资源的关闭和
真实原生线程退出。超时不意味着已完成退出；取消等待不强制终止析构；同一对象
可继续等待。完成优先于已请求的取消。原始非拥有 wrapper／XRT Wait 仍仅排空，
不把这两种层次的合同混为一谈。详情见 [线程退出验收](TaskPool线程退出验收.md)。

## 精确结果与拥有权

TaskGroup 可通过原子 `group.start` 同步调用 `pool.submit` 组合已有生产者，
无需再提供 group 专用池或提交后 add。拥有结果、拒绝回滚及真实 JIT/module/AOT
组合用例见 [模块服务图与 Catalog 验收](模块服务图与Catalog验收.md)。

IR 的新 call-only 借用表示包含原始 Closure、精确结果入口和适配器 CodeOwner。
IR 必须证明固定 nullary 同步回调、回调返回 T 与目标 future<T> 完全相同、精确
payload profile、外部错误／效果合同、借用和原生资源拥有者。独立 verifier 在
输出 C／头文件之前拒绝不一致；不能按相同尺寸或相同指针宽度推测签名。

Cgen 每签名只输出一个 `bool(env, T*)` 表示 thunk 和三字段调用实参，不输出
TaskPool 分配、队列、结果拥有、回滚或清理算法。驻留 `xl6_task_submit_closure.c`
复用 V3 TaskFrame、同一个原生 Job、语言输出 Promise 和 Watch。T 直接写入一个
typed-empty 拥有结果槽，不通过 var 装箱再投影，也不额外复制整个 T。

普通 string／bytes 的语言值语义保持不变：返回拥有值转移到结果槽；之后每次
await 仍按现有规则独立取得结果。可变容器／对象保留它们本来的引用语义。
适配器模块与原始 Closure 模块的租约独立，捕获／结果本身的代码拥有边另行保留。
原生 permanent Drop policy 不借用 producing module 的析构函数。

共同 TaskFrame 还修正了上一批 cleanupError 的真实拥有图边：等待通知期间
如实 trace；通知把该拥有者移到自己的活动范围，再交给失败输出，不能为一个
已经清空但被 collector pin 保留的帧永久保留错误对象或阻塞自身退休。

C 标量缺口在共同 Future 结果层修复，而非 TaskPool 特例：20 种 `c_*` 保留各自
ISO C 存储类型／平台宽度／对齐，不归一化为 int64 或 double。lowering 和 Cgen
共用 canonical IR 分类。Promise、阻塞 await 与恢复式 async 使用同一精确合同。
这不扩大 task.all 已有的更窄的 xvalue 聚合投影域。

## 尚未封板的边界

- 受管理 worker-exit／checked ThreadLocal 已接通。用户析构仍可能阻塞，显式
  无限等待承担该阻塞；期限／取消等待和模块退休的非阻塞准备不会偷偷阻塞 join。
- 普通同步函数按值返回 generator 的现有非复制限制保持。TaskPool 不添加特殊
  clone／move 规则；Future 底层能够保存 generator 不等于该回调签名合法。
- 显式 `func() as null` 的零 payload RETURN 是已有未封板核心问题，本批保留
  明确拒绝，不能生成伪造指针槽。无结果任务使用合法的 `func()`。
- 没有宣称 fresh shipping DLL／SO／VFS、整体并发或五组验收完成；不提升 release。

## 验证入口

证据目录：`F:/xlang-validation/five-groups-20261019/p1-task-pool-language/`。

- `tools/check_stdlib_task_pool_language.py`：冻结完整源码编译器，语言结果／控制／
  取消／错误／self-release／unused／exported borrowed-only／20 C 标量，严格 C
  O0/O2、真实 JIT、模块、独立 AOT，旧 Promise／TaskGroup／Executor 回归；47 个
  错误 IR 在 C/header 输出前拒绝，并检查所有权来源与无默认池／无 Executor 裁剪。
- `tools/check_stdlib_task_submit_closure.py`：真实部署计划和 selected providers，
  每 profile 144 个 body／清理／取消结果、独立 Closure／thunk code owner，显式
  debug profile 另验 72 个 caller OOM 前缀和工作线程结果槽分配失败及精确账本。
  minimal profile 不额外开启 memory_debug。Linux 增加 selected closure sanitizer。
- 原 `check_stdlib_task_pool_control.py` 和 `check_stdlib_task_checked_outcome.py`
  回归真实任务图、400 次运输、新增 cleanupError 真边／pin 退役，以及较早失败
  优先和无诊断失败；坏实现先编译成功，再由运行断言拒绝。

所有输出使用新目录保留失败证据。最终报告必须 Complete、InputsUnchanged、
LogsUnchanged 为 true、Failures 为空，提交前复核实际输入／日志／产物哈希。
无扫描、攻击模拟、网络通信或安全专项。

最终使用 `source-win-final`（619 单元）、`source-linux-final`（618 单元）、
`language-win-r1`／`language-linux-r1` 和两端的 `native/control/outcome-*-final`。
八份测试报告共 408 阶段，含 20 个预期拒绝；两端语言各 92 阶段、各 47 个
输出前错误 IR 拒绝。Linux 的三种选定原生闭包均通过 sanitizer／泄漏检查。
源码构建只复用真实发布 TCC core，不复用旧测试结果；提交前再次核对证据哈希。

以上 408 阶段属于前一批结果接口证据，不代替线程退出批次；后续批次及其
完整源码构建、当前验收与边界单列在 [线程退出验收](TaskPool线程退出验收.md)。
