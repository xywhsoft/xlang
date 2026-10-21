# Dial 的绝对期限与显式取消

已公开异步及同步组合；代理及其他网络范围继续在长期任务中，不因此视为完成。
方法同时存在于 `tcp` 命名空间和 `TcpStream` 静态方法。

```text
dialAsyncCancel(engine, resolver, host, port [, options], cancel)
dialAsyncUntil(engine, resolver, host, port [, options], deadline)
dialAsyncUntilCancel(engine, resolver, host, port [, options], deadline, cancel)

dialAsyncCancel(engine, addresses [, options], cancel)
dialAsyncUntil(engine, addresses [, options], deadline)
dialAsyncUntilCancel(engine, addresses [, options], deadline, cancel)

dialCancel(engine, resolver, host, port [, options], cancel)
dialUntil(engine, resolver, host, port [, options], deadline)
dialUntilCancel(engine, resolver, host, port [, options], deadline, cancel)
dialCancel(engine, addresses [, options], cancel)
dialUntil(engine, addresses [, options], deadline)
dialUntilCancel(engine, addresses [, options], deadline, cancel)
```

异步返回 `future<TcpStream>`，同步直接返回已打开的 `TcpStream`。`cancel` 是 CancellationToken，不接受 CancellationSource 代替。
`deadline` 为与 `time.clock()` 相同单调时钟域的绝对微秒 `uint64`，不是毫秒或墙上时间。
`0` 已到期，`UINT64_MAX` 表示无限。相对连接超时仍由 TcpDialOptions.withTimeoutMillis 控制；
原生连接超时和本次绝对期限都会保留，较早生效的失败／中断按实际交付边界处理。
缺省 options 直接使用原生默认配置，不创建临时语言对象。

```xlang
auto cancellation = CancellationSource()
uint64 due = time.clock() + uint64(5000000)
TcpStream stream = await tcp.dialAsyncUntilCancel(engine, addresses, due, cancellation.token)
```

## 交付合同

驻留实现先创建完整的私有捕获和同一个结果对象，再登记父取消／期限，最后启动真实 XRT Dial。
提交前检查已经观察到取消或到期时，不提交原生 Dial，不执行 DNS；仍在固定工作线程完成 checked 结果和捕获清理。
该检查不是竞态屏障；停止与实际提交竞争时，保留已经提交的真实操作并向其传递取消请求。
显式参数的 host／port／UTF-8 验证仍在提交前完成，不因取消而截断或忽略非法主机名。
端点列表路径不经过 Resolver。

取消不是终态伪造：已经启动的真实源若仍 Pending，输出保持 Pending，直到原生终态可被观察。
父取消为 CancelledError；绝对期限为 TimeoutError。登记失败直接 checked 报错并完整清理。
选择发生在语言就绪队列的回调入口，绝对期限包含该队列的延迟；选择时重新检查时钟，
不依赖定时线程及时得到调度。中断可取代尚未交付的原生成功，但不能撤回已经选择的结果。

原生已经成功而交付前被取消／到期时，不提升结果载体；载体释放会主动中止该未交付连接。
验收保留原生流的额外真实观察引用，确认关闭发生在 Engine 清理之前。
捕获字段元数据复制、类字段身份检查、同一对象提升和 owned 源终态合同复用原有实现。
没有语言 onCancel 闭包、中间 Promise、每等待线程或另一套 Dial 算法。

## ABI、裁剪与验证

同步组合直接复用 XRT 的绝对期限／可取消等待，无语言就绪队列、语言期限服务或每等待线程。
检查已停止时不提交工作；停止与提交竞争时，等待报告的明确取消／超时优先。
原生已完成而尚未交付时，先观察父取消，再检查绝对期限，随后才选择并提升结果。
这规定的是观察顺序，不推断同时发生事件的物理先后；已经选择的结果不被撤回。
停止或等待登记失败后，请求真实源取消，并等待其终态再释放普通拥有者；原生已成功但未交付的连接主动中止。
因此，期限不硬截断必要的安全清理耗时。若底层等待本身失败，实际载体仍保留原生拥有图进入既有 checked 退役／恢复，不伪造终态。
分配／等待登记的首个失败在取消和回收时保留。I/O Worker 上的同步调用在提交前拒绝，即使令牌已取消或期限已过。

受控 native Worker 门禁测试使用真实 Dial/Future：确认源仍 Pending 时不能因外层停止而返回；
独立反向控制删除实际终态等待，必须被该测试拒绝。它不是用假 Promise 或延迟替身模拟真实源。

十二个原生声明的参数拥有者、类字段投影、泛型 Future、checked 错误及 nonblocking 效果均明确登记。
Until 入口要求 executor + deadline 服务；仅 Cancel 的声明只要求 executor，不启动期限线程。
没有期限且没有父取消（Until 的无限期限）复用基本独占源路径。
基本 Dial／同步 Dial／纯枚举不选择本批中断组件；同步仍不启动语言就绪执行器。
适配只在驻留 runtime，Cgen 无按名字猜语义的分支；不修改 XRT、快照或发布二进制。

测试包括实际连接的成功、提交前停止、原生成功后的父取消／到期、字段替换、分配失败和清理；
公开语言覆盖所有十二个重载，host 仅执行提交前停止／参数拒绝并断言 Resolver 提交数为零。
完整源码的 JIT／模块／AOT、严格生成 C、服务和字段合同篡改拒绝及按需裁剪均以冻结报告为准。
只使用离线验证及自有字面量 loopback，不进行公网、系统 DNS、畸形协议或安全专项测试。
小批 `--probe` 仅用于发现问题，不能作为最终验收报告。
未决策维持 CRT 0、XRT 11 组。
