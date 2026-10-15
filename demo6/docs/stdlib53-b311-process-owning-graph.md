# Process 共享拥有图

Process、共享退出 Future 和等待线程使用同一套物理拥有图协议。
收集器必须看见所有实际拥有边；进程退出不等于等待线程已退役，
原生回调也不等于其对象没有拥有边。本次不增加语言标准库 API。

## 物理节点和引用

语言 Process 句柄拥有一个 `XL6_ProcessLive` 引用；该共享包装对象
拥有一个 XRT Process 用户端点。Process 的流适配器也会保留共享包装
对象，不能为每个语言投影复制或估算其引用计数。

XRT Process 的 Count 包含用户端点、后台等待者和收集器 pin 的全部
实际引用。Trace 分别访问 Error、WaitFuture 和 WaitPromise 的拥有槽。
Future 与 Promise 是同一个共享控制块的两个真实引用，因此必须保留
重复边，不能因为目标相同而合并引用数。退出状态快照只包含原生值，
以精确的 payload policy 登记，不把未知 payload 或观察者当作叶节点。

后台等待者执行平台等待时不持有 ownership mutation scope。它在发布
终态和改变拥有槽时进入作用域，通知 Future 和执行回调前退出作用域，
最后再释放自己的真实 Process 引用。该引用存续期间，Process 是外部根；
无需虚构根、扣减后台引用，或根据 EXITED 猜测线程已经结束。

## 冻结和退役

收集器 pin 只保留控制块，不增加 Process 用户端点数。最后一个用户
端点释放时仍须关闭父端管道，让等待 stdin EOF 的子进程继续运行；
释放用户引用不会隐式 terminate 或 kill 子进程。

Clear 只把节点和拥有槽标记为退役，不关闭管道、不等待线程，也不
执行任意回调。实际拥有引用保持到 Finish，所有目标仍由计划的真实
pin 保留。Finish 在冻结区外拆除拥有槽并释放资源；重复 Finish 看到
已经清空的槽，不重复关闭或释放。普通引用计数释放仍可独立工作。

## 异步等待

`Process.waitAsync` 复用已有的驻留 Future 回调帧和经过验证的捕获
布局，不再向共享退出 Future 注册不透明的旧 Then 延续。这个驻留
映射没有语言捕获，也不保留生成代码地址；成功时发布语言异步 ABI
使用的 bool payload，随后由原来的异步函数构造退出状态对象。

每次调用的取消只影响该消费者，不向进程级共享终态 Future 传播。
其他消费者仍可等待退出结果。构造失败会拆除部分 producer、Promise、
token 和源引用，并保留第一次失败。

Future 方法按接收者的静态类型检查，不再要求接收者必须是具名变量。
成员、索引和函数结果复用现有 XLIR 的精确接收者操作数及借用锚点，
只求值一次，并保留调用期间的拥有者。成员与同名全局变量并存时，
用 `self.completion` 明确指定成员，不在 Cgen 重做名称查找。

取消、等待、状态和元数据调用共用一个求值事务，不再各自根据源码
是否为右值创建临时槽。即使第二个参数抛出异常，先求值的 Future
拥有者也由同一失败 CFG 清理。Future 成员字段通过已有的精确、拥有
投影读取，复制引用及其 checked 错误边在 IR 中明确记录。

## 接入和验证范围

ProcessLive 域分别识别共享包装对象与原生 Process 节点；ProcessFuture
域单独登记退出快照的 payload policy。只使用同步 Process 的独立产物
不因此引入异步等待提供方。每个 Future、回调帧和子对象仍需自己的
精确域；注册了未知旧观察者的共享 Future 必须使整个快照拒绝。

`check_stdlib_process_ownership.py` 使用完整源码提供方检查物理节点、
真实循环退役、构造失败前缀、既有语言用例，以及移动后的独立 AOT
析构。XRT 的 `process_ownership_tests` 同时覆盖模块化和单头实现。

Process 派生 Reader、Writer 和 LineReader 的原生上下文仍需独立的
拥有边接入；它们继续拒绝图收集，不继承本次 Process 的证书。
本专项不代替发布库、嵌入 VFS 或全部 53 模块的发布验收。
