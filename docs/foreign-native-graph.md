# 扩展原生组合资源与拥有图

`release/include/xl_foreign_graph.h` 提供显式、版本化的原生组合合同。原有 ForeignResource 仍仅适用于没有隐藏语言/异步拥有边的叶载荷；不能用它包住引擎、TLS verifier 或闭包后跳过追踪。

## 创建与调用

扩展提供只读 Trace、可选 Prepare 和可选原生 Task。Create 复制描述符，成功接管 raw，失败时 raw 仍归调用方。releaser 必须是编译器准入的 `xcall(point)` 语言动作，并与描述符回调处于同一链接产物或它的受管依赖闭包，确保代码真实驻留；任意 C 函数指针不能代替代码能力。releaser 的指针参数载体由运行时预建，最终调用不需要临时分配该参数。

每次访问先 Enter，成功后在 Leave 前使用 scope.raw；Leave 也允许空 scope。不能缓存 raw 或绕过此边界。并发调用、递归重入、已认领或已进入退役的资源均 checked 拒绝。Enter 用原子状态及真实记录引用保护活动调用，不把全局锁跨越回调或同步等待。

## 追踪与退役

Trace 在全域 Freeze 内逐个枚举实际拥有槽，禁止分配、加锁、语言调用或引用计数变更。不能把中间拥有对象扁平化，也不能用代理节点代替真实 owning reference。未知子节点仍 fail closed；目录不会因为外层是合法扩展就接受所有原生对象。

Prepare 在 Freeze 外运行，可返回 BUSY/FAILED/READY。READY 会永久禁止新调用，即使一次整体收集之后恢复 claim，也不恢复已退役的原生资源。Finalize 在清除语言捕获前执行 releaser，最多一次。描述符、releaser 捕获和代码依赖保留到最后物理 pin 释放。清理错误使用现行 checked 错误路径报告。

拥有任务把记录的一个引用交给 `xrtNetEnginePostOwnedV1`，使用驻留 `xl6ForeignGraphTaskPolicyV1Get()`。扩展 Task 返回后，驻留 Drop 才能释放记录与模块代码；在扩展自己的 C 栈尾释放最后代码引用不安全。Task 自行安置完成/错误载体，不能让语言异常越过 C 回调边界。

## 编译依赖

源码扩展在 `artifact.runtimeSymbols` 声明它在 C 内直接调用的驻留符号，以及需要的原生拥有域入口。编译器只接收已登记的符号，展开组件与拥有域依赖，并保留既有原生模块闭包。未知符号整体拒绝，不输出残缺计划；不按文本猜测调用，也不默认引入全部运行时。

网络扩展使用 `xl_foreign_net.h` 的同步 Visit 边界。语言层的 NetEngine/TlsClientContext 先通过编译器已封闭的字段上下文取得真实拥有句柄；驻留端复核精确 HandlePolicy，在调用内保留实际句柄、进入执行作用域，提供短生命周期原生参数。扩展若需要跨调用保留资源，必须取得独立拥有权并追踪实际 owning slot，不能保留 Visit 的借用指针。

## 验证与成本

`tools/check_foreign_graph.py` 包含实际循环、BUSY、重入、两阶段准备、16 个 OOM 前缀与两个独立模块的代码依赖/任务栈尾驻留验证，O0/O2 均保留。目录测试验证未知符号原子失败及原生额外模块不丢失。

记录/指针载体/releaser、作用域原子操作与引用增减均是实际桥接成本。该协议提供明确安全合同，不构成零开销性能声明；具体扩展应与同一版本的手写 C 在同一工作负载下另行测量。
