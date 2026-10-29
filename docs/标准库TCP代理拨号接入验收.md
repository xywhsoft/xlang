# TCP 代理拨号接入与验收

本批次为 P3 的代理拨号公开面，不代表整个网络组或长期任务完成。复用已固定的 XRT Proxy Dial／Future；本批次不修改原生 XRT 算法，不增加 Cgen 生成包装实现。

## 公开接口

`TcpStream` 新增以下方法，每项均提供默认配置与显式 `TcpProxyDialOptions` 两个重载，共 16 项。

| 同步 | 返回 `future<TcpStream>` |
| --- | --- |
| `dialProxy` | `dialProxyAsync` |
| `dialProxyCancel` | `dialProxyAsyncCancel` |
| `dialProxyUntil` | `dialProxyAsyncUntil` |
| `dialProxyUntilCancel` | `dialProxyAsyncUntilCancel` |

共同参数为 `NetEngine engine, NetResolver resolver, NetProxy proxy, string host, int port`；显式配置位于其后，最后依次为绝对期限和取消令牌（按方法需要）。代理类型由现有 `NetProxy` 的 SOCKS5／HTTP CONNECT 配置选择；凭据和认证规则复用现有接口。

```xl
auto engine = NetEngine(NetEngineOptions().withBackend(NetBackend.select))
engine.start()
auto resolver = NetResolver()
auto proxy = NetProxy(NetProxyType.socks5, "127.0.0.1", 1080)
auto options = TcpProxyDialOptions().withTimeoutMillis(5000)
TcpStream stream = await TcpStream.dialProxyAsync(engine, resolver, proxy, "example.test", 443, options)
```

此段仅展示调用形式，不用于验收；实际验收自己创建本机回环代理，不访问上述目标或系统 DNS。

`TcpProxyDialOptions` 是不可变、按值复制的原生配置对象：

- `transportOptions()`／`withTransportOptions(TcpDialOptions)`：内层端点解析和 TCP 策略。
- `timeoutMillis`／`withTimeoutMillis(int)`：全过程默认 30000 ms；必须为正数或 `-1`；`-1` 关闭外层总期限，不关闭内层策略。
- `receiveLimit`／`withReceiveLimit(uint64)`：握手接收上限默认 65536，必须为正且适合本机 `size_t`。

期限参数采用既有单调时钟绝对微秒合同，与配置的相对毫秒不同。传输地址族只约束代理端点；目标名称经校验后交给代理，不在本机解析，也不因代理连接使用 IPv4 而拒绝合法目标名称。

## 拥有权与执行合同

- 复用既有 Stream carrier、真实 Engine／Resolver 父引用、类字段投影和 checked 退役；没有另造一套生命周期。
- 原生提交先把实际 source Future 放入 carrier，再交付额外引用；临时提交参数只在同步构造期间借用。
- 异步完成在固定工作线程的就绪队列执行，不为每次等待创建协程栈或系统线程。
- 取消／期限通过既有中断控制器传递给实际 source；失败清理不把“发出取消”冒充原生终止。
- 同步入口拒绝在所属 I/O worker 上阻塞；同步-only 程序不因此引入语言就绪队列。
- 关闭后的 `waitAsync` 是状态观察，与既有同步等待一致；`recvAsync` 仍是新数据操作，继续受关闭状态限制。保留的 capture 维持真实原生子对象的生命周期。

原有 Dial 的 prepare／submit／promote 和同步 finish 工具提取为共享驻留单元；普通 Dial 与 Proxy provider 保持独立，避免相互拉入无用桥接代码。

## 裁剪与编译器合同

新桥接均有确定的参数类型、服务需求、错误／阻塞效果、原生句柄策略和类投影合同。Cgen 仍只负责声明、调用和拥有清理。

配置对象本身不引入协议状态机；实际代理拨号明确引入 SOCKS5 与 HTTP CONNECT，因为 `NetProxy` 类型在运行时选择协议。不会因此引入 TLS、TaskPool、协程调度器或普通 TCP Dial Future provider。

实现期间独立 AOT 验证发现协议实现未进入精确裁剪集合，已在 component 依赖声明补齐，而非在 Cgen 或生成 C 中补丁处理。

## 验收入口与边界

`tools/check_stdlib_tcp_proxy_source.py`：当前完整源码构建、五个正例、五个语义拒绝例；生成 C 的 GCC O0／O2、JIT、模块、独立裁剪 AOT、动态导入审计，以及真实 XLIR 合同变异检查。同步／异步夹具覆盖所有 16 个重载、SOCKS5 匿名及认证、HTTP CONNECT、隧道预读数据、内嵌 NUL、预取消、实际等待取消和过期期限。

`tools/check_stdlib_tcp_proxy_runtime.py`：实际拥有图、真实 Resolver 父引用、原生 source 和 Proxy 退役；每次运行包含 16 种图清理顺序与 64 个构造分配预算，并重跑原有 TCP 资源夹具。GCC／TCC 的 O0／O2，select 与 IOCP／epoll；Linux 另跑 ASan／UBSan。移除实际 root、完成策略、Stream 事件策略登记的负控制必须被拒绝。

`tools/check_stdlib_tcp_language_runtime.py --dial-sync-controls` 和 `tools/check_stdlib_tcp_language_source.py --dial-sync-controls` 回归原有 TCP／Dial、取消、期限、同步终止等待和语言队列裁剪。同步 join 负控制跟随共享工具移至 `xl6_tcp_dial_sync_common.c`，没有删除原有检查。

2026-10-21 的证据根目录：`F:/xlang-validation/five-groups-20261021/p3-proxy-public/`。正式报告分别保存在 `source-accept-*`、`runtime-*`、`dial-source-*`、`dial-resident-*` 下的 `results.json`；仅 `Complete=true` 且输入、日志、产物哈希均未变化的报告计入完成。`discovery-*` 为问题发现记录，不计正式验收。

本次八份正式报告均已完成，合计 540 个验收阶段（包含预期拒绝项）：

| 范围 | Windows | Linux |
| --- | ---: | ---: |
| Proxy 当前源码／生成 C／JIT／模块／独立 AOT／XLIR | 56 | 56 |
| Proxy 驻留拥有图／分配失败／精确策略负控制 | 25 | 28 |
| 原有 TCP／Dial 当前源码回归 | 131 | 131 |
| 原有 TCP／Dial 驻留 ABI／中断／清理回归 | 52 | 61 |

Proxy 的两个优化级别在每个平台均检查 338 个实际桥接目标，拒绝 1498 次描述符变异；九类合同违反均由完整验证器在 C／头文件输出前拦截。Windows 完整源驱动含 750 个编译单元，Linux 含 749 个（不编译 Windows 专用 UTF-8 I/O shim）。上述阶段计数不把完整源驱动构建或问题发现轮次重复计入。

本批次只验收离线合同、分配失败和自建字面量回环正常通信／取消／关闭。不执行外网、系统 DNS、畸形协议、扫描或网络安全测试。当前源码驱动的成功不等于发布 DLL／SO／VFS 已重建；不把其他工作区前置、扩展库或 IDE 改动纳入本批次提交。

## 未决策分类

CRT 为 0 项；XRT 仍有 11 组，未在本批次改动决策：core／error／ownership；内存；动态值与图；其余平台信息；HTTP；WebSocket；运行时类型／转换；对象／字段图；调用／value 桥接；typed 容器；内部支撑。
