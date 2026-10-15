# Process 派生 IO 的拥有图

Process 的 Reader、Writer 和 LineReader 通过实际流上下文持有共享
Process 包装对象。它们接入同一个拥有图，不再因为包含回调而被一律
拒绝，也不能因为回调驻留就被误判为没有拥有边的叶子。

## 唯一资源与共享引用

XRT Reader、Writer、LineReader 及私有 ProcessStream 上下文采用唯一
拥有，不含独立引用计数。语言句柄的 Trace 穿过这条唯一资源链，访问
上下文实际保留的那一个 Process 包装引用。contextOwner 是该引用的
借入元数据，不增加引用，不为唯一资源虚构 Count 或额外图节点。

同一 Process 的多个流分别贡献真实拥有边；共享包装对象仍拥有一个
原生 Process 用户端点。原生等待线程、Future 和 Promise 的引用继续
按各自真实计数和已有策略处理，不扣除外部线程引用。

## 转移和关闭

Reader 转为 LineReader 时，原生 Reader 和上下文边在同一 mutation
scope 内转移。成功后旧 Reader 为空，只有 LineReader 追踪该边；分配
或参数失败时原 Reader 和边均不改变。关闭先在短作用域内同时拆除
原生流槽和边，再在作用域外执行 Close 回调及资源释放。

已关闭或已转移的已知句柄不再拥有 Process 边。未知来源、自称进程流
却缺少边、或夹带未获目录准入的上下文仍须拒绝；不能运行未知节点的
Count 或 Trace，也不能默认把它当作叶子。

## 可重试完成

Clear 保留原生句柄和流，用户析构仍在 Clear 前访问完整字段。共享
Process 包装对象的 Finish 通过现有 completion 协议检查真实流租约：
只要租约存在就返回 BUSY，不拆除原生 Process 端点。流的 Finish 关闭
上下文并释放租约后，后续 Finish 才释放原生端点。

这个顺序不依赖图节点的发现或遍历顺序。完成步骤可重试，已完成的
资源不会再次释放；不使用自动 kill、隐藏的引用折扣或合成根。

## 验证范围

`check_stdlib_process_ownership.py --process-io` 检查精确物理图、失败
转移、未知上下文拒绝、流租约与循环退役，以及构造失败前缀。原生
专项以真实分配账目等待后台线程的释放尾部，不把进程终态视为线程
已退出。语言夹具还在析构中读取拥有字段、传递内嵌 NUL 和 UTF-8，
并通过公开模块生命周期及移动后的独立 AOT 检查退役。

普通 IO 的组件闭包不因本次接入引入 Process。任意宿主流、任务组、
终端竞态、完整 exit 清理和正式产品发布验收仍保留各自的接入及验证
要求；本专项不扩大公开标准库 API。
