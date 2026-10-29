# TCP 传输配置

`TcpStreamOptions`、`TcpDialOptions`、`TcpListenOptions` 是完整、不可变的
原生配置值。构造和 with 方法只验证并复制配置，不创建 Engine、Socket、连接
或线程，不绑定端口，不进行 DNS。它们是 P3 真正传输接入的前置，不是已完成
Dial／TCP／Server／UDP 接入的替代品。

## 流配置

```xlang
auto stream = TcpStreamOptions()
stream = stream.withReadBuffer(4096, 1048576)
stream = stream.withWriteWatermarks(262144, 65536, 1048576)
stream = stream.withConnectTimeoutMillis(30000).withKeepAlive(true)
```

| 读取属性 | 更新入口 |
| --- | --- |
| `readSize`、`readLimit` | `withReadBuffer(chunkSize, limit)` |
| `writeHighWater`、`writeLowWater`、`writeLimit` | `withWriteWatermarks(high, low, limit)` |
| `connectTimeoutMillis` | `withConnectTimeoutMillis(millis)` |
| `readMode` | `withReadMode(TcpReadMode)`；adaptive／direct／probe |
| `noDelay` | `withNoDelay(bool)` |
| `keepAlive` | `withKeepAlive(bool)` |

容量使用 uint64，进入原生 size_t 前检查可表示范围。chunkSize 必须为正，
limit 不得小于 chunkSize；写入要求 `0 < high <= limit`、`0 <= low <= high`。
相关字段一次性更新，避免为了调整水位而被迫经过不合法的中间配置。

## 拨号策略

`TcpDialOptions()` 提供 `streamOptions()` 和 `withStreamOptions(options)`，
以及以下读取属性和对应更新方法：

| 属性 | 更新方法 |
| --- | --- |
| `family` | `withFamily(0|4|6)`：双栈／IPv4／IPv6 |
| `affinity` | `withAffinity(uint64)` |
| `timeoutMillis` | `withTimeoutMillis(int)` |
| `fallbackDelayMillis` | `withFallbackDelayMillis(int)` |
| `maxAttempts` | `withMaxAttempts(int)`；原生当前上限 64 |

超时覆盖原生 Dial 的总期限，流配置保存每个数字地址尝试的策略。fallback
延迟允许零，但不能为负。创建策略本身不解析主机、不启动地址竞争，也没有
偷偷使用默认 Resolver。主机、端口、代理和传输服务由后续实际 Dial 入口组合。

## 监听配置

`TcpListenOptions()` 使用 XRT 的平台默认绑定策略和 IPv4 动态端口；
`TcpListenOptions(NetAddress)` 使用独立地址副本，不借用调用者对象。

- `address()` 返回拥有型 NetAddress；`withAddress(address)` 返回新配置。
- `streamOptions()` 返回独立流配置；`withStreamOptions(options)` 复制嵌套值。
- `affinity`／`withAffinity(uint64)`。
- `acceptConcurrency`、`acceptQueueLimit`／`withAcceptQueue(concurrency, pendingLimit)`。
  并发范围 1—1024，排队上限为正 uint32 可表示值。
- `backlog`／`withBacklog(int)`；正数且不超过原生 int 范围。
- `distribution`／`withDistribution(TcpAcceptDistribution)`；roundRobin／local。
- `reuseAddress`、`reusePort`、`exclusiveAddress`、`ipv6Only`／
  `withBinding(reuseAddress, reusePort, exclusiveAddress, ipv6Only)`。
  独占与两种复用不能同时启用。

平台默认值由固定 XRT 初始化，不人为抹平 Windows 和 POSIX 的绑定差异。
withBinding 一次更新完整绑定策略，避免四个可变 setter 产生竞态或中间冲突。
地址对象不能通过这里被隐式解析、修改或保留裸引用。

## 时间与原生 ABI

语言公开单位统一为毫秒。xlang 当前固定的 TCP 原生快照仍是 uint64 微秒，
原生零表示无限；独立 XRT 仓库已具有更新的有符号毫秒 ABI，不能整包混用。

本批只引入两个不改变结构布局的校验入口，保留固定 ABI：

- 连接／总超时使用 -1 表示无限；有限值必须为正，乘 1000 前检查溢出。
- 此固定 ABI 无法表示“有限零超时”，因此显式拒绝零，不把它悄悄变成无限。
- fallbackDelayMillis 是有限延迟，零保持零，其余非负值检查转换溢出。
- 从宿主原生配置取得的非整毫秒值，查询时向上取整；配置副本仍保留精确微秒，
  不把显示单位的舍入写回底层值。

未来升级原生时间 ABI 必须作为显式迁移处理，不能借补函数顺带改变现有布局。

## 合同、生命周期和生成代码

所有 with 方法返回独立新值，不修改原对象。嵌套配置按值复制；返回的地址和
流快照在源配置销毁后仍有效。三个拥有域都是无语言边的真实配置叶对象：没有
回调、Engine、调用者地址或外来池的隐藏引用。这不授权把实际活动传输也标为叶。

校验复用 XRT 的 Stream／Listener／Dial 创建规则，不在语言层再实现一份。
新增 `xrtNetStreamConfigValid`、`xrtNetListenConfigValid` 只是公开既有谓词的
checked 入口，空指针拒绝；没有复制算法或引入实际 I/O。语言边界另外检查
窄化和时间单位，这不是第二套原生配置规则。

52 个闭合驻留声明核对精确标量、名义枚举、错误、效果和拥有关系。只读标量
查询 pure／no-raise／nonblocking；分配快照和 with 方法允许 checked 错误。
Cgen 只生成声明、直接调用和已有拥有清理，不输出配置实现体。ABI 头为平面
声明，不要求独立 SDK 携带私有 .inc 文件。

七个驻留组件分开根入流／拨号／监听／地址投影与三个拥有域。单独流、监听
不选择 Dial；单独标量监听查询不选择地址分配投影；仅使用枚举不选择 TCP。
配置复用现有 XRT TCP 模块及其原生依赖，不承诺独立裁掉其全部原生算法体；
但不根入语言 Engine／Resolver／ExecutionSource／默认服务，也不创建这些资源。

## 验收与未完成范围

入口是 `tools/check_stdlib_tcp_options_runtime.py` 和
`tools/check_stdlib_tcp_options_source.py`，证据目录为
`F:/xlang-validation/five-groups-20261021/p3-tcp-config`。

本批覆盖完整字段、组合规则、原生单位转换、源对象销毁后快照、实际拥有图、
分配失败、真实组件计划，以及 JIT／模块／严格 C／独立 AOT。没有 Engine／
Socket 创建、DNS、数据包或网络安全性质测试；只更新必要声明头和符号快照，
不替换发布二进制或 VFS。

原生校验入口来自 XRT 提交 `b2a7f6d4`。其隔离源码／单头测试 Windows 16、
Linux 20 阶段通过，GCC／TCC O0／O2，Linux 含实际选定闭包 ASan／UBSan。
只提交两个谓词入口和相应头／测试／文档，未纳入 XRT 文件或内存库的未提交改动。

驻留五种实际闭包 Windows 59、Linux 69 阶段通过，同样含双编译器／优化等级、
真实拥有检查、20 个拥有分配失败前缀，以及 Linux 选定闭包完整 ASan／UBSan。
源码驱动 722／721 个翻译单元；语言两端各 148 阶段通过，包括 9 个新用例、
3 个保留回归、12 条语义／IR 负例、四种实际裁剪计划和无运行库动态依赖的 AOT。
O0／O2 各检查 52 个目标、4 个枚举位置、26 个拥有型结果，拒绝 478 种损坏
合同且不输出 C／头；完整语言对象各运行 359 个分配失败前缀，首次不命中 358，
全部账本平衡。仅使用枚举的计划不根入 TCP。

证据为 `native-win-r2`／`native-linux-r2`、`runtime-win-r1`／`runtime-linux-r1`、
`source-win-r2`／`source-linux-r1`、`language-win-r1`／`language-linux-r1`。
原生早期 r1 和 GCC O0 驻留发现记录保留；Windows source-win-r1 误用旧版
PowerShell，在编译前因缺少 GetRelativePath 终止，不计为通过；改用现有 pwsh
并在全新目录构建，没有修改产品来适配旧测试宿主。

旧 TcpConnectConfig／TcpListenConfig 的真正传输绑定、Stream／Listener／Dial／
Server／UDP 的原生完整拥有图及 checked 退役、Future 结果与取消、代理拨号、
文件和任务组合仍待 P3 后续完成。不得用不透明裸句柄或伪造叶策略绕过这些条件。
