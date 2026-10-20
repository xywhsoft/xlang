# Executor 标准库接入

`import concurrent` 按需提供受管理 `Executor` 与不可变 `ExecutorSnapshot`。
已接通单项与原子批量提交、配置、关闭／取消、等待、首错／计数和统计的实际语言路径。
公开 TaskPool 已另批接入，见 [TaskPool 标准库](TaskPool标准库接入.md)。完整
shipping DLL／SO／VFS 联合验收与后续五组工作仍未完成，不能据此宣称完整
并发接入或整个长期任务完成。

```xl
import concurrent

auto count = AtomicInt64()
auto executor = Executor(2, 64)
executor.submit(func()
    count.fetchAdd(1)
end)
executor.close()
assert executor.wait(10000)
assert count.load() == 1
```

## 公开合同

构造为 `Executor(threads = 0, queueLimit = 1024, stackBytes = 0)`。零线程数
使用 XRT 的原生自动配置；非零线程数须可表示为 uint32。`queueLimit` 是
**每个工作线程**的正排队上限，不是总容量；与 XRT 一致，总排队容量为工作
线程数乘此上限。stackBytes 为非负原生栈预算，零表示原生默认值；全部转换
先校验范围，不能截断后再创建线程。

`submit(func() work)` 只接受零参数、无返回值的 checked 语言闭包。提交失败
走普通异常边，拒绝的作业不执行。没有返回 Future；需要拥有结果的任务使用
后续 TaskPool／既有 task 体系，不把 detached 回调伪装成返回值任务。

`submitBatch(array<func()> work)` 使用同一零参数 void 闭包合同。一次调用只做
一次原生原子准入：整批接受，或全部拒绝；不拆成多次单项提交，也不因为容量不足
悄悄切批。原生批次容量限制仍适用，容量不足走 checked 错误边，不执行拒绝批次
中的任何 body。开放执行器接受空数组作为 no-op；关闭后即使空批次也拒绝。

准备阶段在短期 Array 操作 pin 内保留实际 callable box，之后先结束输入 pin，
再进入原生准入。接受的作业不借用输入数组的槽；调用者可以立即清空数组，回调
自身也可以清空来源数组。正在执行的同一闭包可以再次提交；这不保证其捕获数据
自动线程安全，跨线程共享状态仍须使用同步能力。批次中每个元素独立计入作业统计
和共用失败账本，不新增“批次失败次数”或 Future。

`close()` 停止新提交，仍执行已经接受的工作；`cancel()` 同时取消排队工作，
不强杀正在运行的回调。关闭可重复，仍可读统计、等待和报告错误。`wait(ms)`
要求先关闭／取消，-1 无限、0 轮询、正数为有限毫秒；真正完成返回 true，
超时返回 false，错误仍抛出。原生完成包含捕获清理和错误登记，不只 body 返回。

ThreadLocal 跨同一真实工作线程上的多次作业保留，不按作业清空。关闭并排空后，
线程退出使用驻留 checked 清理入口；`wait()` 包含这段尾部。每个失败的线程退出
记一次失败，和作业共用首错／计数账本；原生 submitted／completed 等仍只计作业。
没有选择 ThreadLocal 的程序仅携带可选退出分派，不因此拉入 ThreadLocal 实现。

`failureCount`／`failureCountSaturated` 保留全部历史失败；首错按完整作业或线程
退出清理结束后的登记顺序，未必是提交顺序。正常取消不算失败，取消时的捕获清理失败仍算。
`wait()` 完成后和 `throwIfFailed()` 显式报告首错，不清掉历史首错／计数，因此
可以重复检查，也不重新包装错误。无已分配诊断的失败仍由独立 bool／checked
合同传递，不能解释为成功。

需要区分历史与通知：显式报告已经交付给语言的错误，不在模块退出时再变成
一遍未捕获异常。新的失败会重新产生待报告通知，计数饱和后也不丢通知。
从未显式报告的失败仍由普通资源退休或 whole-graph Finish 向宿主 checked
清理报告，不静默丢弃。图 Finish 先交付一次原始失败，随后可重试完成物理职责，
不重复通知；宿主保留失败结果，所有真实 pin 在职责完成前继续存在。

`snapshot()` 复制一份 native 统计：threads、queueLimit、queued、running、
submitted、completed、executed、stolen、cancelled、rejected、closed、cancelling。
快照不持有来源执行器。失败计数另读，不承诺它和快照中的 native 统计跨两个
账本形成同一瞬间的原子快照。

## 声明、XLIR 与生成 C

标准库只声明 extern。驻留 `xl6_executor_language.c` 负责已验证语言 ABI 与
物理资源 API 的适配，`xl6_execution_executor.c` 复用 XRT 算法与账本。
调用前后错误／拥有转换沿现有通用 XLIR 规则，没有新 Cgen 执行器实现函数。

构造需要的模块源由闭合合同明确标记：

```c
xl6ExecutorCreateLanguageV1(
    (XL6_ExecutionServicesV1){xlir_execution_source}, threads, limit, stack);
```

`ExecutionServicesV1` 是只借用一次调用的精确服务视图，不修改既有 continuation／
deadline 服务 ABI。新的 `executionOwnershipAbi` 从可达 CALL／动态方法合同重算，
校验器独立检查；不能用一个相同 C 指针、符号前缀或缓存 ABI 标记获得能力。
零参数 void 闭包、拥有字段、错误物理合同也在发射之前严格校验。

批量调用使用已封闭的 Array／callable 存储合同：

```c
xl6ExecutorSubmitBatchLanguageV1(
    (XL6_NullaryVoidArrayContextV1){array_identity, signature_identity},
    executor_handle, work);
```

两个 identity 是编译器验证过的类型元数据，不是 Array 布局猜测。接入驻留
`xl6_executor_batch.c`，复用原生 `xrtExecutorSubmitOwnedBatchV1`；生成 C 中
不出现准备循环、临时 Closure 数组、原生队列算法或本地包装函数体。仅借用调用方
Executor 的模块不因为调用 submitBatch 而创建自己的 Execution Source。

每个实际需要它的模块拥有一个 `xlir_execution_source` creator 槽，并报告真实
拥有边。初始化只安装完整源；失败的部分模块仍可 checked 退休。Source 独立于
默认 async 执行器、期限源和旧内部 TaskPool。关闭准入在语言全局清理之前，
排空／销毁在全局拥有者释放之后；BUSY 保留源槽，不重复已经提交的 IR finalizer。
与网络服务组合时，先完成可能使用网络的执行资源尾部，再退休依赖的网络服务。

whole-graph 清理可能已经 Clear／Finish 源，所以 Source.Close 必须能重复调用
已关闭／已物理退休但仍有 creator／检查 pin 的源，不能访问已释放的事件。
资源本身的最后引用退休、检查 pin、原生 creator、图暂停／恢复协议见
[执行资源生命周期接入合同](执行资源生命周期接入合同.md)。

未使用的声明不创建源，不拉入 XRT Executor；单独使用 Executor 也不隐含引入
默认 async 执行器或 TaskPool。调用 Channel 有限异步接口时，三个源分别拥有，
不能因为它们都使用线程而混同生命周期。

## 验证边界

维护入口 `tools/check_stdlib_executor_language.py --source-build <build.json>
--out <新目录>`；Linux 另传实际 TCC 宿主 `--native-root`。
使用完整当前源码构建的编译器、驻留实现和 SDK，不替换查找 hook；仅复用
真实的 TCC 核心。它不证明新 shipping DLL／SO／内嵌 VFS 已发布或整体 RC0。

覆盖实际 JIT、模块、独立 AOT、生成 C 的 GCC O0／O2、配置／期限／取消、
重复显式报错、未报告失败、工作线程释放最后语言引用、未使用声明裁剪，以及
Channel continuation／deadline 组合。白盒真实 XLIR 拒绝 15 种服务、闭包、错误
及模块拥有合同变异；批量合同另拒绝 17 种实际 XLIR 变异，C／头发射都不能
先交付部分输出。错误参数／返回值的 callable 数组也有语言负例。

`check_stdlib_execution_resource.py` 另覆盖完整实际原生／provider 的 GCC／TCC
O0／O2、Linux sanitizer／泄漏检查、Source.Close 在图清理后的幂等性，以及
64 个普通／图退休的已报告／未报告、NULL／已分配首错组合。两个新增实际坏实现
验证“图通知丢失”和“重复报告已处理失败”确实能被拒绝。
这些仍不是所有 OS 失败或任意并发交错的穷尽证明；没有执行网络通信或安全专项。
显式 Thread 与受管理 Executor 已分别在真实线程退出边界接通 checked hook；
这不代表 TaskPool 或所有默认异步工作线程都已获得同样的语言接入验收。
最近的批量验收记录见 [Executor 原子批量提交验收](Executor原子批量提交验收.md)。
