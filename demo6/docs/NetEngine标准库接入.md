# NetEngine 标准库接入

本批接通 Engine 的配置、独立生命周期、工作投递、Timer、状态和统计；不等同于
完成 Socket／Resolver／TCP／UDP／TLS，也不代表 shipping DLL／VFS 已验收。

## 使用

```xlang
auto options = NetEngineOptions().withWorkers(2).withBackend(NetBackend.select)
auto engine = NetEngine(options)
engine.start()
engine.post(func()
    // 短小的非阻塞工作，不占用另一个线程／协程栈。
end)
auto timer = engine.after(1000, func()
    // 在所属 Worker 上执行。
end)
engine.cancelTimer(timer)
engine.close()
assert engine.wait(5000)
```

`net.engine(workerCount = 0)` 等价于使用默认配置创建停止态 Engine。
`NetEngine()` 与 `NetEngine(options)` 均有直接驻留声明。没有公开 point 构造、
adopt、WorkerPort 或“原生统计指针”逃逸口。

## 生命周期与错误

- `start()` 启动实际固定 Worker。`stop()`／`tryStop()` 停止后允许再次启动。
- `stop()` 不等待仍存活的 Socket／Resolver 等原生子对象，存在时 checked 拒绝；
  最后语言引用释放只请求异步退役，不保证下一条语句时已完成。最终退出使用
  `close()` 后 checked `wait()` 等待退役，不能以 sleep 代替完成条件。
- `tryStop()` 返回是否完成物理停止；BUSY 不等于错误。实际错误仍 checked 报告。
- `close()` 是不可逆的**语言投递准入关闭**，不单独承诺此调用已停止原生循环。
  已受理工作继续有效；调用 `wait()` 或释放最后语言拥有引用推进实际停机。
- `wait(timeoutMillis = -1)` 要求已 close；返回 false 表示期限届满，可重试。
  -1 表示无限等待，0 为立即尝试。不能在自身 Worker 上等待／停止。
- 生命周期控制独占，重叠控制及 Worker 重入检查返回 StateError，不持有元数据锁
  执行回调／join。这里不会创建一个额外 Engine 控制线程；复用模块 ExecutionSource。
- `post(work, affinity = 0)` 与 `after(delayMillis, work, affinity = 0)` 只接受
  无参、无返回值闭包。after 返回非零 Timer ID，延迟不得为负。
- `cancelTimer(id)` 只确认取消请求被接受，不承诺已经执行完成，也不回滚已运行 body。
  正常取消／停机取消只释放捕获，不算工作失败；真正的生产者失败会登记。
- 已接受工作的 body 和捕获清理合为一次失败结果；保留首个错误，累计 failureCount，
  溢出通过 failureCountSaturated 表明。`wait()`、完成的 stop／tryStop 与
  `throwIfFailed()` 显式报告；观察后仍可再次取得同一首错。拒绝投递不算工作失败。
- 遗弃未观察的失败不会静默成功；模块 checked 退役报告失败并保留必要清理责任。
  不将工作异常改成 abort，不使用空诊断指针代替独立失败位。

## 不可变配置及快照

`NetEngineOptions` 的每个 with 方法返回独立的新值，不修改原配置。覆盖 Workers、
Backend、CommandCapacity、NodeCacheBytes、TimerLimit、EventBatch、PortPostLimit、
PortWatchLimit、PortOperationLimit、PortOperationCache、ThreadStack，以及四个缓冲
尺寸类及共享缓存预算。实际硬边界复用 XRT ConfigValidate，不另造一套 native 约束。

idleWaitMillis 只接受正毫秒或 -1（无限）；0 被拒绝，避免固定 XRT 的零无限约定被
误解为轮询。原生微秒配置复制时保留原值；语言毫秒读取向上取整。

`configuration()` 复制请求配置与嵌套缓冲配置，不保留原对象地址。
`backend(worker = 0)` 和 `capabilities(worker = 0)` 查询运行中的实际 Worker 后端。
编译可用与启动可用不同；显式选择失败不回退到其它后端。

`snapshot()`／`net.stats(engine)` 返回拥有型不可变 NetEngineStats。它可比 Engine
活得更久；公开状态使用封闭 NetEngineState 枚举。快照没有公开构造器、裸句柄或
无初始化的非空默认实例。二十项统计字段由统一描述表生成，不通过反射逐名查询。

## 结构与裁剪

适配在驻留 runtime；标准库为声明，IR 独立核验句柄拥有者、效果、服务注入、回调与
封闭枚举，Cgen 只负责调用和正常拥有清理。配置／快照不根入 ExecutionSource；
Engine 不根入 Executor、TaskPool、默认异步执行器、Resolver、TCP 或 TLS。
原生 ConfigValidate 当前属于 NET_ENGINE 编译模块，不能将配置裁剪描述为完全没有
Engine 原生算法。网络的微秒／绝对期限 ABI 未迁移。

Engine、任务／Timer、闭包捕获、失败账本与执行源使用同一真实拥有图。精确的原生
Ops／quiescence 与策略身份先验证，再枚举边。Clear 后按任意 Finish 顺序消费真实
拥有引用。闭包环境以明确状态保证恰好一次 Drop，包含合法 NULL 环境与重入清理。

## 验证边界

专项工具：`check_stdlib_net_engine_runtime.py`、`check_stdlib_net_engine_language.py`；
共享回归：`check_stdlib_execution_resource.py`、`check_stdlib_executor_jobs.py`，以及
公开 Executor／NetBackend 全源码验收。Windows／Linux 均使用 GCC／TCC O0/O2，
Linux 另做选中闭包的 ASan／UBSan／泄漏检查。

只测试自己的 Worker、Timer 和拥有图，不创建 socket，不查询外部 DNS，不探测
服务，不做网络安全专项。证据位于
`F:/xlang-validation/five-groups-20261020/p2-engine-runtime`；失败轮和输入变化轮
保持留存，不计正式通过。最终通过轮次与 SHA 汇总以该目录提交回执为准。

原生前置提交 `ca085682`；固定快照只同步 net.h／engine.c 两节，并保存独立 backport
回执。现有用户的其它 XRT、IDE、扩展库和构建模板修改不纳入本批提交。

最终冻结源码驱动为 Windows `source-win-r6`（647 编译单元）、Linux
`source-linux-r1`（646 编译单元）。正式通过的限定报告如下：

| 专项 | Windows | Linux |
| --- | --- | --- |
| Engine 实际运行时／配置／拥有图 | accepted-runtime-win-r1，42 阶段 | accepted-runtime-linux-r1，44 阶段 |
| 共享执行资源／TaskPool | resource-win-r2，84 阶段 | resource-linux-r2，86 阶段 |
| 共享脱离式工作节点／Executor | jobs-win-r1，35 阶段 | jobs-linux-r1，37 阶段 |
| Engine JIT／module／AOT／IR | language-win-r3，73 阶段 | language-linux-r2，73 阶段 |
| 既有公开 Executor／线程本地清理 | executor-win-r1，115 阶段 | executor-linux-r1，115 阶段 |
| 既有 NetBackend／地址回归 | backends-win-r1，31 阶段 | backends-linux-r1，31 阶段 |
| XRT 原生与固定消费端 | regress-win-r1，32 阶段 | regress-linux-r1，32 阶段 |

Engine 每端每种优化独立拒绝 450 项无效 IR 合同，六个正向 AOT 无 xlang 共享库依赖。
负例按具体原因断言，合法 point 输入不能调用 Engine 构造器；不能将更早的非法类型
转换拒绝冒充该合同的证明。最终提交回执会重算所有正式报告的输入／日志／产物摘要，
并保留失败轮与所有无关工作区文件。shipping 源清单四配置均为 603／603；这仅证明
源提供者清单完整，不替代尚未进行的 shipping 产品验收。
