# 网络后端能力查询

2026-10-20，P2 的 Engine／后端选择前置。限定双平台源码验收完成；
不据此宣称 Engine、Socket、Resolver、网络 40 模块或五组长期任务完成。

## 公开 API

```xl
auto backend = net.defaultBackend()
assert net.backendCompiled(backend)
auto caps = net.backendCapabilities(backend)
if (caps & net.capability.sendFile) != 0 then
    console.writeLine(net.backendName(backend))
end
```

`NetBackend` 是独立的数值枚举：`automatic`、`iocp`、`ioUring`、`epoll`、
`kqueue`、`select`。选后端的参数不接受普通整数，避免把容量或其他枚举混入。

| 入口 | 合同 |
|---|---|
| `net.backendCompiled(NetBackend)` → bool | 当前产物是否编入该后端；automatic 检查默认选择器是否可选到具体驱动 |
| `net.backendCapabilities(NetBackend)` → int | 已编译驱动的能力位；未编入时为 0，查询本身不抛错 |
| `net.backendName(NetBackend)` → string | 原生名称的独立拥有字符串；未编入报 LookupError，分配失败不交付半成品 |
| `net.defaultBackend()` → NetBackend | 同一原生选择器解析 automatic 后的具体驱动；没有驱动时报 LookupError |

能力位是可组合的整数集合，不冒充普通枚举。`net.capability` 下提供 `none`、
`readiness`、`completion`、`oneShot`、`edge`、`batch`、`wake`、`post`、
`cancel`、`readProbe`、`datagramError`、`sendFile`、`fileIO`，数值与 XRT 一致。
查询 `backendCompiled/backendCapabilities` 声明 pure、no-raise、nonblocking，
不分配、也不覆盖调用前错误；名称复制和可能失败的默认查询保持 checked 合同。

## “已编译”与“可启动”

这不是 OS 可用性探测。查询不创建端口／线程／Socket，不读取远端地址，也不
尝试初始化 io_uring。内核版本、权限及资源条件仍由真正的端口／Engine 启动
确认。显式选择不支持的后端不能因此自动换成 Select；完整 Engine 的创建、
实际后端／能力快照和 checked 退出仍须在后续批次接通。

驻留组件明确选择语言发行面支持的五类驱动，实际编译体由 XRT 的平台条件
决定：Windows 为 IOCP／Select，Linux 为 Epoll／io_uring／Select，BSD/macOS
为 Kqueue／Select。automatic 完全沿用 XRT 优先级，不在语言层复制策略。
其它平台保留 XRT 自身支持的 Select。能力位描述驱动声明，不能把 readiness
误认成原生 completion，或把用户态文件发送冒充系统 sendfile。

原生补充只有 `xrtNetPortBackendInfo` 和数据记录；复用现有驱动选择器，不
另造后端注册表。名称借用仅存在于原生调用内部，语言结果独立复制。按精确
来源差量同步固定 XRT/xruntime 两套实现和声明，没有升级其它网络超时 ABI。
原生带驱动和无驱动产品均允许查询：只有 port 核心时全部可为 false；Select
单驱动产品的 automatic 则解析为 Select。

## 组件与验收

标量查询和名称字符串适配分别驻留。仅查能力位不选择名称复制组件；没有
语言原生拥有体或 callback，不需要伪造 leaf policy。两种入口都不启动隐式
Engine／Resolver，也不选择 DNS／TLS。Cgen 只使用精确直接调用合同，没有
新增这些功能的实现函数体。

直接调用合同同时补齐通用、显式选择的数值枚举描述符。它核对规范声明身份、
底层整数表示、完整成员名及值，再允许该枚举按普通标量 ABI 传入／返回。
不把同宽 int、其它枚举或篡改的成员映射当作相同合同，不修改 Cgen，也不
放宽原有基础标量入口。NetBackend 的六项词汇只是声明数据，不是后端特判。

证据根：`F:/xlang-validation/five-groups-20261020/p2-net-backends`。
正式源码构建为 `source-win-r7`（636 编译单元）与 `source-linux-r5`（635）；
报告为 `backends-win-r3/results.json`、`backends-linux-r1/results.json`，每端
31 阶段、1 个精确类型错误预期拒绝。输入、日志与产物摘要一致；独立 AOT
导入在 `seal.py` 中复核，不依赖 xl/xlc DLL／SO。没有替换 shipping 产品。

每端每种优化还拒绝 46 个无效 IR 合同，在 C/header 输出前守住枚举身份、
整数替换、表示宽度、成员名／值、效果及服务依赖。查询零分配、保留已有
错误和名称复制的一个分配故障前缀通过，精确内存账本平衡。标量用例的
实际组件计划排除名称复制；全部公开路径排除 Engine／Resolver／DNS／TLS。

原生报告为 `native-win-r3/results.json`、`native-linux-r3/results.json`，
每端 30 阶段：O0/O2 下分别验证无驱动核心、Select-only、平台后端全集，
用模块声明编译测试与实际 port.c，并链接生成实现执行元数据测试。原生
独立提交为 `470dc51d`；生成头只提交此次差量，保留既有无关改动。

早期诊断、输入变化、宿主 I/O／资源失败和错误检查器断言的轮次均保留，
不计通过；增量诊断对象不进入正式源码证明。公开验收使用真实 native
提供者，无替身、网络 I/O、外部查询或 OS 安全策略修改。
