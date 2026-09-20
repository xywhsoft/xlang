# XLIR 原生 OS 请求拥有链

2026-09-19，v217。IOCP／io_uring 完成式请求已接入显式拥有合同，
真实 XLIR 回收规划器也已覆盖待决请求、取消终态与物理排空。
这补的是语言对象被原生异步操作访问时的寿命语义，不是扩展标准库功能。
readiness watch、完整 Engine／transport 和原 18 个生成服务图门仍开放。

## 实现边界

16 个公开 OwnedV1 入口和 3 个内部文件入口共用 13 组原有校验逻辑。
成功受理消费一个实际 Context 引用，失败不消费；不根据 User 猜测拥有关系，
也不通过线程隐式状态附加引用。Context 必须真正保护 socket、数据缓冲、
文件句柄、User 与回调代码。描述符、地址和控制值仍由后端按原合同复制。
SendMsg 的空控制分支复用同一个 lease，不丢掉或重复消费引用。

端口拥有链是实际引用存储，不是供图检查使用的影子链。OS 描述符只借用 lease。
受理成功后在 mutation 内将 lease 接入端口锁保护的拥有槽；取消只发起请求，
不会摘掉拥有槽。实际完成先生成私有事件，再在 mutation 和端口锁内将拥有槽
原子转给输出事件。已经发布的事件即使遇到后续 Wait 错误，也必须交给调用者。
提交失败路径释放尚未发布的 lease，不调用 Context.Drop。

io_uring 的原请求完成与取消控制 CQE 可以先后到达。有效负载终态已经交付后，
后端允许保留私有描述符等待控制尾部；该尾部不访问 Context 或 socket，不能
为方便图检查再添加虚假引用。端口销毁仍先完成真实后端排空，再释放拥有槽。
Accepted socket 独立拥有；事件 release 不代替关闭它。旧借用请求仍令端口
拒绝完整图准入，Engine 的裸端口拒绝和 Cgen 的完整服务图拒绝均未删除。

## 集中发现与修复

- 新夹具最初误用了不存在的截断事件标志，且循环书写触发严格缩进警告。
  已改为原 API 的 TRUNCATED 结果码，并同时验证字节数、前缀和非 ERROR 标志。
- 单独后端预检的两组参数缺少平台选择所需特性宏，已分别保存为命令配置错误，
  不归因于运行时实现。
- Linux 模块化夹具缺少 errno／unistd 头，正常严格 C11 构建还暴露 mkstemp
  所需 POSIX 特性声明。已在夹具首部补齐；没有通过放宽警告或改变运行时构建
  默认值掩盖问题。失败源码、两种 GCC 未运行结果与 TCC 通过结果全部保留。

## 已完成验证

Windows 聚焦报告 `xlir_launch_v217_native_repaired/results.json`：
GCC O0／O2、TCC 各 46/46。该报告早于仅影响 Linux 的夹具可移植性修正；
运行时单头与最终候选逐字节一致。新程序验证 TCP／UDP、scatter/gather、
覆写临时描述符、ReadProbe、两种空控制 SendMsg、截断、取消、OOM 回滚、
真实文件读写／SendFile 和“完成后再报错”的事件交付。

Linux 报告 `xlir_launch_v217_linux_native_repaired/results.json`：
WSL 的 GCC O0／O2、TCC 各 11/11，均实际运行 io_uring，不是编译通过或跳过。
每个新拥有权程序有 92 次完成、4 次取消、4 组文件操作、4 次部分交付错误，
以及 106 次跨线程冻结回调探针；包含真实 ICMP 错误队列完成。
活动分配、字节、分配／释放差额及非法释放计数精确平衡。此处不是全 Linux
平台认证；不支持后端时入口返回 77，不能计为通过。

持久入口 `tools/check_xlir_native_port_plan.cjs` 仍链接未经修改的生产
`xl6_ownership_plan.c`。Windows 保留全部原 64 个 Post 场景，增加 64 个真实
OS 请求场景，三配置各 128 轮；Linux 三配置各 96 轮。每通道新增场景实际
交付 136 个取消终态，其余请求经销毁排空。外部事件引用保活、精确策略拒绝、
析构复活、真实 BUSY、collector pin 和回调冻结探针全部保留。
报告为 `xlir_launch_v217_port_plan{,_linux}_discovery/results.json`。

同一单头候选的 Engine／Buffer／TaskNet 规划器三配置分别各 192、96、80 轮
通过，见 `xlir_launch_v217_{engine,buffer,task_net}_plan_verified/results.json`。
唯一一次 Windows 扩大回归已完成：GCC／TCC 各 313/313，0 失败、0 未运行，
保留 v216 全部 307 程序，批次输入前后相同。报告为
`xlir_launch_v217_native_verified/results.json`，最终封存见 v217 checkpoint。

## 下一步和保留边界

1. 为 Select／epoll／kqueue 建立明确的 watch 代次拥有槽。分方向 one-shot、
   同批合并、替换后旧事件和当前 watch 必须持有真实共享 lease 引用。
2. 迁移 TCP／UDP／TLS continuation 的旧 self-drop 协议，补齐宿主真实
   Count／Trace、缓冲和事件存储，再完成 Engine 的完整图准入。
3. 将已认证原生合同接回语言服务 ABI，解决原 18 个生成模块场景；不得只删除
   Cgen 的 live-service 拒绝。语言集成证据仍为 v209，需重核输入后复用。

混合对象寿命、DLL 卸载、完整语言／FFI、同源发布／隔离部署、其他平台和
旧路径退役仍待验收。之前被隔离的 ResolverFuture／layout TCC 专项缺口不由
本轮端口 TCC 通过替代。本轮不覆盖生产嵌入头、发布 DLL，也不切换默认 Cgen。
