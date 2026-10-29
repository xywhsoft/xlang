# Set 显式键策略底座

本批为原生 Set 接入补齐“相等＋哈希”键策略，不给引用类型伪造排序能力。
公开语言 API 不变；语言 Set/Dict carrier 迁移、容器键变更约束、拥有图适配和
跨组发布验收仍未完成，DEV-XRT-53 不关闭。

## XRT 合同

`xrtTypedSetInitWithKeyPolicy` / `xrtTypedSetCreateWithKeyPolicy` 接受不可变的
`xtypedsetkeypolicy`：Hash、Equal 以及借用的 UserData。元素类型继续提供初始化、
复制、移动、释放和追踪；必须有效、占用非零大小且可复制，但不必具备排序比较。
普通 Init/Create 与默认对象 InstanceOps 仍保留原有 Compare/Hash 要求。

策略记录、回调代码和上下文须比原集合及所有 clone/集合代数结果存活更久。
克隆沿用原策略，二元操作同时校验精确元素描述、两种回调和上下文地址；不同策略
不能仅凭相同元素类型合并。篡改 Storage 策略会被 typed API 拒绝。

`xtypedset` 增加策略指针，C ABI 大小改变，所有调用方需要同步重编译。
旧 InstanceSize 不能通过负载类型验证。测试专用 memory-debug 依赖没有进入
生产 typed_set 的模块闭包。

## xlang 驻留策略

私有 `xl6SetReferenceKeyPolicyV1` 使用真实 XRT Value Hash/Equal，C NULL 与
XRT null 表示同一缺省值。字符串、bytes 按真实长度处理，内嵌 NUL 不截断。
复制元素保留语言引用，生命周期使用共享 retained-reference 类型，不借用生成
模块的代码或上下文。该类型的 Compare/Hash 能力仍为空。

`xl6SetFutureKeyPolicyV1` 按实际 Future 控制块身份计算键；完成状态和最终结果
不会改变键语义。槽位生命周期仍使用 `xrtTypeFuture()`，不创建伪 Value 对象。

引用键回调复用共同递归预算和 checked 错误边界。正常的零哈希、false 相等
判断不算失败；无诊断的失败先保留给语言父边界，再提供原生拒绝诊断，不能被
转换为合法键结果。同一集合回调重入仍被拒绝。

驻留策略作为独立按需 `SET_KEYS` 组件登记。它仅提供键能力，不授权错误元素
类型，不解决可变内容键的生命周期约束，也不宣布语言 Set/Dict 已迁移。

## 同步与验证边界

供应方与消费方使用精确基线的 typed_set 两个源码段同步。Core 的时间 ABI 和
其他模块源码不随此次同步更新。快照新增两个底层 API；元数据、符号表和原生
链接目录同步生成。对外标准库函数清单与未决策项保持不变。

验证覆盖严格 C O0/O2、Windows/Linux、模块化与单头、旧 typed Set/Dict 回归，
以及原生驻留策略的 NUL/UTF-8/bytes/null、引用保留、Future 身份、递归上限、
重入、无诊断错误和完整 OOM 前缀。真实实现变异必须先成功编译，再被相同向量
拒绝。Linux sanitizer 范围是选定原生闭包，不是整个编译器或发布库。

旧重入探针在未修改的供应方 HEAD 已复现失败：它记录预期拒绝，却留下该错误，
导致外层操作正确失败。夹具现在消费自己已处理的预期错误，同时保留独立拒绝
断言；生产实现的重入与错误检查没有因此放宽。

本批证据位于 `E:/xlang-validation/stdlib53-b186-*` 和 Linux 对应的
`/var/tmp/xlang-stdlib53-b186-*`，较早失败尝试保留，不计入验收。
CRT 未决策 0 项，XRT 未决策 19 项。共享默认子对象仍仅按声明 clone 能力隔离，
否则 checked 拒绝；signal 注册线程显式事件泵与对象池最后开发的决策不变。
